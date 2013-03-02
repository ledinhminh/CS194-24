Feature: Multithreaded HTTP Server
  
  Background:
    Given the root path is "http://localhost:8088"

  @start
  Scenario: Get the test page
		When I visit "/test.html"
		Then I should see "A test page"

	@start
	Scenario: Get the Lorem page
		When I visit "/lorem.html"
		Then I should see "Quisque sit amet congue elit"

	@start
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

	@start
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