class File
  def wait_open(filename, options)
    while (!File.exists?(filename))
      sleep(1)
    end

    return File.open(filename, options)
  end
end
