#pragma once

#include <set>
#include <string>
#include <vector>

// Declaration

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings);

std::vector<std::string> SplitIntoWords(const std::string& text);

//Implementation

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	using namespace std;
	set<string> non_empty_strings;
	for (const string& str : strings) {
		if (!str.empty()) {
			non_empty_strings.insert(str);
		}
	}
	return non_empty_strings;
}
