@multi
Feature: Multithreaded HTTP Server
  
  Background:
    Given the root path is "http://localhost:8088"

  Scenario: Get the test page
		When I visit "/test.html"
		Then I should see "A test page"

	Scenario: Get the Lorem page
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"

	Scenario: Get same page multiple times
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/test.html"
		Then I should see "A test page"

	Scenario: Get different pages multiple times
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"
		When I visit "/test.html"
		Then I should see "A test page"
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"

	Scenario: Check CGI
		When I visit "webclock"
		Then I should see "00 +0000"

	Scenario: Check multiple CGI requests
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "webclock"
		Then I should see "00 +0000"

	Scenario: Check interleaved CGI and static reqeusts
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"
		When I visit "webclock"
		Then I should see "00 +0000"
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"		
