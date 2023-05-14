#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result = 0;
	cin >> result;
	ReadLine();
	return result;
}

vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		}
		else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}

	return words;
}

struct Document {
	int id;
	double relevance;
};

class SearchServer {
public:
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document) {
		if (document.empty()) return;
		++document_count_;
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double term_frequency = 1. / words.size();
		for (const string& word : words) {
			word_to_document_freqs_[word][document_id] += term_frequency;
		}

	}

	vector<Document> FindTopDocuments(const string& raw_query) const {
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				return lhs.relevance > rhs.relevance;
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

private:
	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	struct QueryWord {
		string data;
		bool is_minus;
		bool is_stop;
	};

	map<string, map<int, double>> word_to_document_freqs_;
	set<string> stop_words_;
	int document_count_ = 0;

	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	QueryWord ParseQueryWord(string text) const {
		bool is_minus = false;
		//Word shouldn't be empty
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}
		return { text, is_minus, IsStopWord(text) };
	}

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWordsNoStop(text)) {
			const QueryWord query_word = ParseQueryWord(word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					query.minus_words.insert(query_word.data);
				}
				else {
					query.plus_words.insert(query_word.data);
				}
			}
		}
		return query;
	}

	double ComputeInverseDocumentFreq(const string& word) const {
		return log(static_cast<double>(document_count_) / word_to_document_freqs_.at(word).size());
	}


	vector<Document> FindAllDocuments(const Query& query) const {
		vector<Document> matched_documents;
		map<int, double> document_to_relevance;
		for (const string& plus_word : query.plus_words) {
			if (word_to_document_freqs_.count(plus_word) == 0) {
				continue;
			}
			const double inverse_document_frequency = ComputeInverseDocumentFreq(plus_word);
			for (const auto& [id, frequency] : word_to_document_freqs_.at(plus_word)) {
				document_to_relevance[id] += frequency * inverse_document_frequency;
			}
		}
		//Исключение документов, в которых встречено минус-слово
		for (const auto& minus_word : query.minus_words) {
			vector<int> id_to_delate;
			if (word_to_document_freqs_.count(minus_word) == 1) {
				for (const auto& [id, frequency] : word_to_document_freqs_.at(minus_word))
					id_to_delate.push_back(id);
			}
			for (const auto& id : id_to_delate) {
				document_to_relevance.erase(id);
			}
		}
		for (const auto& [id, relevance] : document_to_relevance) {
			matched_documents.push_back({ id,relevance });
		}
		return matched_documents;
	}
};

SearchServer CreateSearchServer() {
	SearchServer search_server;
	search_server.SetStopWords(ReadLine());
	const int document_count = ReadLineWithNumber();
	for (int document_id = 0; document_id < document_count; ++document_id) {
		search_server.AddDocument(document_id, ReadLine());
	}
	return search_server;
}

int main() {
	const SearchServer search_server = CreateSearchServer();
	const string query = ReadLine();

	for (const auto& document : search_server.FindTopDocuments(query)) {
		cout << "{ document_id = "s << document.id << ", "
			<< "relevance = "s << document.relevance << " }"s << endl;
	}
}