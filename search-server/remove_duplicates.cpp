#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
	map<set<string>, int> storage;
	vector<int> ids_for_remove;

	for (const auto document_id : search_server) {
		const auto& word_frequencies = search_server.GetWordFrequencies(document_id);
		set<string> storage_key;
		transform(word_frequencies.begin(), word_frequencies.end(),
			inserter(storage_key, storage_key.begin()),
			[](const auto& item) { return string(item.first); });

		if (storage.count(storage_key) > 0) {
			ids_for_remove.emplace_back(document_id);
		}
		else {
			storage.insert({ storage_key, document_id });
		}
	}

	for (auto document_id : ids_for_remove) {
		cout << "Found duplicate document id "s << document_id << '\n';
		search_server.RemoveDocument(document_id);
	}
}