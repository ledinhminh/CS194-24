# This code abstracts away getting a single line of output from QEMU.
# There are two important considerations here: that the QEMU instance
# can be killed asynchronusly by the watchdog timer, and that the
# watchdog needs to be reset whenever a line gets properly read.
def next_line()
  # We need exclusive access to @line_thread as there is a race
  # between the watchdog thread killing the thread and this code
  # creating the thread.
  @line_lock.lock

  # If the subprocess has already been killed then we can't possibly
  # proceed, any test asking for output must be already failed
  if (@qemu_out_pipe == nil)
    @link_lock.unlock
    fail
  end

  # Start a thread that gets a single line -- this exists because we
  # might want to kill the waiting request.  I'm shying away from
  # using non-blocking IO here with the hope that this will make
  # writing tests easier
  @line = nil
  @line_thread = Thread.new{
    @line = @qemu_out_pipe.gets

    # I'm considering the race here to be unimportant -- is there
    # really any difference between a watchdog that runs for N seconds
    # vs a watchdog that runs for N seconds minus a few instructions?
    # I think not.
    @watchdog_set = true
  }

  # Now that the thread has been started up there is no longer a race
  # condition
  @line_lock.unlock

  # Wait for the thread to finish, implicitly using the ending
  # condition that "@line == nil" to indicate that the thread was
  # killed prematurely
  @line_thread.join
  @line_thread = nil
  if (@line == nil)
    fail
  end
  
  return @line
end

# Writes a single line to QEMU.  This just forces a flush after every
# line -- while it's not particularly good for performance reasons, we
# need it for interactivity.
def write_line(line)
  @qemu_in_pipe.puts(line)
  @qemu_in_pipe.flush
end

# This fetches a single line from the kernel but ensures that it's not
# been given a printk-looking message
def next_line_noprintk()
  while (/\[ *[0-9]*\.[0-9]*\] /.match(next_line()))
  end

  return @line
end

# Kills the currently running QEMU instance in an entirely safe manner
def kill_qemu()
  # We need a lock here to avoid the race between starting up the
  # thread that reads from QEMU and killing it.
  @line_lock.lock
  
  # First, go ahead and kill the QEMU process we started earlier.
  # Also clean up the pipes that QEMU made
  Process.kill('INT', @qemu_process.pid)

  # NOTE: it's very important we DON'T cleanup the sockets here, as
  # something else might still need access to them.  I should really
  # figure out Cucumber's pre and post hooks so we can clean things
  # up.
  #`./boot_qemu --cleanup`
  
  # Killing this thread causes any outstanding read request.  This
  # will result in a test failure, but I can't just do it right now
  # because then this test will still block forever
  if (@line_thread != nil)
    @line_thread.kill
    @line_thread = nil
  end
  
  # Ensure nobody else attempts to communicate with the now defunct
  # QEMU process
  @qemu_out_pipe = nil
  @qemu_in_pipe = nil
  @qemu_mout_pipe = nil
  @qemu_min_pipe = nil
  running = false
  
  # This critical section could probably be shrunk, but we're just
  # killing everything so I've err'd a bit on the safe side here.
  @line_lock.unlock

  # Informs the file waiting code that it should stop trying to wait
  @qemu_running = false
end

# This waits for a file before opening it.  I can't figure out why I
# can't call this "File.wait_open()", which is what I think it should
# be called...
def file_wait_open(filename, options)
  while (!File.exists?(filename) && @qemu_running)
    STDERR.puts "Waiting on #{filename}"
    sleep(1)
  end
  
  return File.open(filename, options)
end

def boot_linux(boot_args)
# This lock ensures we aren't brining down the VM while also trying
  # to read a line from stdout -- this will cause the read to block
  # forever
  @line_lock = Mutex.new

  # Start up QEMU in the background, note the implicit "--pipe"
  # argument that gets added here that causes QEMU to redirect
  `./boot_qemu --cleanup`
  @qemu_process = IO.popen("./boot_qemu --pipe #{boot_args}", "r")
  @qemu_running = true

  # This ensures that QEMU hasn't terminated without us knowing about
  # it, which would cause the tests to hang on reading.
  @qemu_watcher = Thread.new{
    puts @qemu_process.readlines
    kill_qemu()
  }

  # FIXME: Wait for a second so the FIFO pipes get created, this
  # should probably poll or something, but I'm lazy
  sleep(1)
  @qemu_in_pipe = file_wait_open("qemu_serial_pipe.in", "w+")
  @qemu_out_pipe = file_wait_open("qemu_serial_pipe.out", "r+")
  @qemu_min_pipe = file_wait_open("qemu_monitor_pipe.in", "w+")
  @qemu_mout_pipe = file_wait_open("qemu_monitor_pipe.out", "r+")

  # Skip QEMU's help message
  @monitor_thread_read = false
  @monitor_thread = Thread.new{
    @qemu_mout_pipe.gets
    @monitor_thread_read = true
  }

  # Start up a watchdog timer, this ensures that the Linux system
  # doesn't just hang.  This has the side effect of killing the
  # instance whenever it doesn't produce output for a while, but I
  # guess that's OK...
  @watchdog_set = false
  @watchdog_thread = Thread.new{
    running = true
    while (running)
      # Tune this interval based on how often messages must come in.
      sleep(10)
      
      # If no messages have come in between this check and the last
      # check then go ahead and kill QEMU -- it must be hung
      if (@watchdog_set == false)
        kill_qemu()
      end
      
      # Clear the watchdog, it'll have to get set again before another
      # timer round expires otherwise we'll end up killing QEMU!
      @watchdog_set = false
    end
  }

  # Reads from the input pipe until we get a message saying that Linux
  # has initialized.  It's very important that any init the user
  # supplies prints out this message (or kernel panics) otherwise
  # we'll end up just spinning while waiting for an already
  # initialized Linux.
  init_regex = /^\[cs194-24\] init running/
  panic_regex = /^\[ *[0-9]*\.[0-9]*\] Kernel panic - not syncing/
  running = true
  while (running)
    next_line()

    if (init_regex.match(@line))
      running = false
    elsif (panic_regex.match(@line))
      STDERR.puts("kernel panic during init: #{@line}")
      running = false
    end
  end

  # Ensure the monitor thread has actually gotten a line from the QEMU
  # monitor -- otherwise we'll be all out of sync later...
  if (@monitor_thread_read == false)
    fail
  end
end

def run_cmd(cmd)
  # Run the command
  write_line(cmd)
  # It will be echo'd back by the serial terminal emulation we're
  # using
  next_line()
  # And then it will also print out a confirmation -- this is  
  # extremenly important because this is the only way to enforce
  # synchronization between the VM and the host, oherwise we'll end up
  # with the serial buffer breaking all the ordering (so, for example,
  # when we want to dump some memory it'll get all lost)
  STDERR.puts next_line()

  #sleep to wait for stuff to run
  sleep(3)
end

Given /^Linux is booted with "(.*?)"$/ do |boot_args|
  boot_linux(boot_args)
end

Then /^the extra version should be "(.*?)"$/ do |extra_version|
  # We just need to read the output, this special init already prints
  # out the version information
  line = next_line_noprintk()

  # We can finally test the actual version here.
  if !(/Linux \(none\) [0-9]*\.[0-9]*\.[0-9]*#{extra_version}/.match(line))
    fail
  end
end

Then /^Linux should shut down cleanly$/ do
  # Look for Linux's power-off message
  while !(/\[ *[0-9]*\.[0-9]*\] Power down\./.match(next_line()))
    STDERR.puts(@line)
    # A clean shutdown means that the code can't kernel panic.
    if (/^\[.*\] Kernel panic/.match(@line))
      fail
    end
  end

  # Clean up after any potential left over QEMU cruft
  `./boot_qemu --cleanup`
end

When /^the "(.*?)" command is issued$/ do |command|
  run_cmd(command)
end

Then /^Qemu gets killed$/ do
  kill_qemu()
end

Before do 
  if !$dunit 
    # do it
    boot_linux("")
    run_cmd("httpd")
    $dunit = true 
  end 
end 