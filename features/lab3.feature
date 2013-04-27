Feature: Able to access test1.html

  Ensure that we are able to use eth194 to ping each node

  Scenario: One eth194 can ping an ne2k_pci device
    Given Initialized tests
    Given Linux is booted with "--net ne2k_pci,macaddr=0A:0A:0A:0A:0A:0A"
    And Linux2 is booted with "--net eth194,macaddr=0A:0A:0A:0A:0A:0B --node2"
    Then Linux should shut down cleanly
    And Linux2 should shut down cleanly
    And Cleanup Qemu cruft
