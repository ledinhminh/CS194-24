Feature: Able to access test1.html

  Ensure that we are able to use eth194 to ping each node

  Scenario: One eth194 can ping an ne2k_pci device
    Given Initialize tests
    And Linux is booted with "--net ne2k_pci,macaddr=0A:0A:0A:0A:0A:0A"
    And Linux2 is booted with "--net eth194plus,macaddr=0A:0A:0A:0A:0B:0B --node2"
    Then Linux1 ping Linux2 10 times
    And Linux2 ping Linux1 10 times
    Then Linux should shut down cleanly
    And Linux2 should shut down cleanly
    And Cleanup Qemu cruft

  Scenario: One eth194 can ping an eth194 device
    Given Initialize tests
    And Linux is booted with "--net eth194plus,macaddr=0A:0A:0A:0A:0A:0A"
    And Linux2 is booted with "--net eth194plus,macaddr=0A:0A:0A:0A:0B:0B --node2"
    Then Linux1 ping Linux2 10 times
    And Linux2 ping Linux1 10 times
    Then Linux should shut down cleanly
    And Linux2 should shut down cleanly
    And Cleanup Qemu cruft

  Scenario: Now lets attempt to send lorem.html to host
    Given Initialize tests
    And Linux is booted with "--net ne2k_pci,macaddr=0A:0A:0A:0A:0A:0A --redir tcp:8088::8088"
    Then Linux1 execute "nc -l -p 9000 > test"
    And Host execute "cat www/test.html | netcat 127.0.0.1 9000"
    Then Linux1 execute "diff test /var/www/test.html"
    Then Linux1 execute "echo zzzDONE"
    Then Linux1 output does not contain "+++" stop at "zzzDONE"
    Then Linux should shut down cleanly
    And Cleanup Qemu cruft

  Scenario: zzzCleanup
    Then Cleanup Qemu cruft
