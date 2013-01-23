@lab0.0
Feature: CS194-24 Version String

    Seeing as how we'll be running modified Linux kernels for this
    class, we're going to go ahead and tag our sources with an extra
    version number to distinguish them from the mainline.

    Scenario: Check Version
        Given Linux is booted with "--initrd .lab0/version_initrd.gz"
        Then the extra version should be "cs194"
	And Linux should shut down cleanly
