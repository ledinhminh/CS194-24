Given /^the root path is "(.*?)"$/ do |url|
  Capybara.app_host = url
end

When /^I am on the root page$/ do
    visit '/'
end

When /^I (visit|am on) "(.*?)"$/ do |url|
    visit url
end

Then /^I should not see "(.*?)"$/ do |text|
    page.should have_no_content text
end

Then /^I should see "(.*?)"$/ do |text|
    page.should have_content text 
end

When /^I search for "(.*?)"$/ do |keyword|
  fill_in 'gbqfq', :with => keyword
  click_button 'gbqfba'
end

Then /^show me the page$/ do
  save_and_open_page
end

