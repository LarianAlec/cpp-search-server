#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iomanip>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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
        } else {
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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};


//template<typename A, typename B> auto Predicat(A, B, A);

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template< typename DocumentPredicat >
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicat document_predicat) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicat);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const{
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status,  int rating) {
            return status==DocumentStatus::ACTUAL;
        });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_predicat, int rating) {
            return status_predicat == status;
        });
    }


    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template< typename DocumentPredicat >
    vector<Document> FindAllDocuments(const Query& query,  DocumentPredicat document_predicat) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicat(document_id, 
                    document_data.status, 
                    document_data.rating )) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// -------------------------Реализация макросов -----------------
// ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST 

template <typename Container>
ostream& Print(ostream& out, const Container& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", ";
        }
        is_first = false;
        out << element;
    }
    return out;
}

template <typename Key, typename Value>
ostream& Print(ostream& out, const map<Key, Value>& map_container) {
    bool is_first = true;
    for (const auto& [key, value] : map_container) {
        if (!is_first) {
            out << ", ";
        }
        is_first = false;
        out << key << ": " << value;
    }
    return out;
} 

template <typename RandomType>
ostream& operator<<(ostream& out, const vector<RandomType>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename RandomType>
ostream& operator<<(ostream& out, const set<RandomType>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, map<Key, Value> map_container) {
    out << "{"s;
    Print(out, map_container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(TestFunc test, const string& func_name) {
    test();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }

}

// Добавление документов. Добавленный документ должен находиться по поисковому
// запросу, который содержит слова из документа
void TestAddDocumentsAndFindDocument() {
    const int doc_id1 = 42;
    const int doc_id2 = 4;
    const string content1 = "cat in the city"s;
    const string content2 = "dog near bus station in the city"s;

    const vector<int> ratings = {1, 2, 3};
    // Перед добавлением документа список документов пуст. После добавления
    // документа список документов увеличивается в размере
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 2);
    }

    //При добавлении нескольких документов с одинаковым id  список документов увеличиваться не должен
    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content2, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
    }

    // Добавленный документ должен находиться по поисковому запросу
    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs1 = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs1.size(), 1);
        const Document& first_doc1 = found_docs1[0];
        ASSERT_EQUAL(first_doc1.id, doc_id1);
   
        // если в поисковом запросе присутсвует слово, встречающееся
        // в нескольких документах, выдаются несколько документов, 
        // причем на первом месте находится документ с наибольшим 
        // пересеченеием слов
        const auto found_docs2 = server.FindTopDocuments("dog in the city"s);
        ASSERT_EQUAL(found_docs2.size(), 2);
        const Document& first_doc2 = found_docs2[0];
        ASSERT_EQUAL(first_doc2.id, doc_id2);
    }

}


void TestMinusWordsInQueryExludeDocumentFromResult() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const string raw_query = "cat -city"s;

    // Документы, содержащие минус-слова поискового запроса, не должны
    // включаться в результаты поиска.
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto find_docs = server.FindTopDocuments(raw_query);
        ASSERT(find_docs.empty());
    }
}

 
void TestMatchingDocuments() {
    const int doc_id = 42;
    const string content = "fluffy cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    // При матчинге документа по поисковому запросу должны быть возвращены все слова
    // из поискового запроса, присутствующие в документе.
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const vector<string> words = get<vector<string>>(server.MatchDocument("fluffy cat", doc_id));
        ASSERT_EQUAL(words.size(), 2);
    }
    // Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const vector<string> words = get<vector<string>>(server.MatchDocument("fluffy cat -city", doc_id));
        ASSERT_EQUAL(words.size(), 0);
    }
}

void TestSortByRelevance() {
    //doc1
    const int doc_id1 = 42;
    const string content1 = "fluffy cat in the city"s;
    //doc2
    const int doc_id2 = 4;
    const string content2 = "black dog near bus station in the city";
    //doc3
    const int doc_id3 = 12;
    const string content3 = "black fluffy cat in the city";
    const vector<int> ratings = {1, 2, 3};

    const string query = "black fluffy cat in the city";
    // Возвращаемые при поиске документов результаты должны быть 
    // отсортированы в порядке убывания релевантности.
    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
        const auto documents = server.FindTopDocuments(query);

        //самый релевантный документ должен быть doc3
        ASSERT_EQUAL(documents[0].id, doc_id3);
        ASSERT_EQUAL(documents[1].id, doc_id1);
        ASSERT_EQUAL(documents[2].id, doc_id2);
    }
}

void TestRatingIsEqualArithmeticMeanOfTheDocumentsRatings() {
    const int doc_id = 42;
    const string content = "fluffy cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const Document& document = server.FindTopDocuments("fluffy cat in the city")[0];

        // вычисление среднего арифметического
        int sum = 0;
        for (int i = 0; i < static_cast<int>(ratings.size()); ++i) {
            sum += ratings[i];
        }
        const int arithmetic_mean = sum/static_cast<int>(ratings.size());

        ASSERT_EQUAL(document.rating, arithmetic_mean);
    }
}

void TestPredicatFiltrating() {
    const vector<int> rating = {1, 2, 3};
    const string content = "cat in the city"s;
    const string query = "cat in the city"s;

    // с помощью предиката должен быть реализован поиск по лямбда-функции
    {  
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::ACTUAL, rating);
        server.AddDocument(2, content, DocumentStatus::IRRELEVANT, rating);
        server.AddDocument(3, content, DocumentStatus::BANNED, rating);
        server.AddDocument(4, content, DocumentStatus::REMOVED, rating);

        vector<Document> even_docs = server.FindTopDocuments(query, []( int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) {
            return document_id % 2 == 0;
        });
        ASSERT_EQUAL_HINT(even_docs.size(), 2, "Вывод документов с четным id с помощью предиката"s);
        ASSERT_EQUAL(even_docs[0].id, 2);
        ASSERT_EQUAL(even_docs[1].id, 4);

        vector<Document> irrelevant_docs = server.FindTopDocuments(query, []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) {
            return status == DocumentStatus::IRRELEVANT;
        });
        ASSERT_EQUAL_HINT(static_cast<int>(irrelevant_docs.size()), 1, "Вывод документов по статусу с помощью предиката"s);
        ASSERT_EQUAL_HINT(irrelevant_docs[0].id, 2, "В данном тестовом случае документ со статусом IRRELEVANT имеет id = 2"s);
    }

    //поиск документов с указанием статуса
    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::BANNED, rating);
        server.AddDocument(2, content, DocumentStatus::BANNED, rating);
        server.AddDocument(3, content, DocumentStatus::BANNED, rating);
        server.AddDocument(4, content, DocumentStatus::REMOVED, rating);

        vector<Document> banned_docs = server.FindTopDocuments(query, DocumentStatus::BANNED);
        vector<Document> removed_docs = server.FindTopDocuments(query, DocumentStatus::REMOVED);
        ASSERT_EQUAL(static_cast<int>(banned_docs.size()), 3);
        ASSERT_EQUAL(static_cast<int>(removed_docs.size()), 1); 
    }

    //поиск документов без указания статуса
    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::ACTUAL, rating);
        server.AddDocument(2, content, DocumentStatus::ACTUAL, rating);
        server.AddDocument(3, content, DocumentStatus::ACTUAL, rating);
        server.AddDocument(4, content, DocumentStatus::REMOVED, rating);
        vector<Document> actual_docs = server.FindTopDocuments(query);
        ASSERT_EQUAL(static_cast<int>(actual_docs.size()), 3);
    }
}

void TestFiltratingByStatus() {
    const vector<int> rating = {1, 2, 3};
    const string content = "cat in the city"s;
    const string query = "cat in the city"s;

    // с помощью предиката должен реализован поиск по статусу
    {
        SearchServer server;
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::ACTUAL).size()*1, 0);
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::BANNED).size()*1, 0);
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::IRRELEVANT).size()*1, 0);
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::REMOVED).size()*1, 0);

        server.AddDocument(1, content, DocumentStatus::ACTUAL, rating);
        server.AddDocument(2, content, DocumentStatus::IRRELEVANT, rating);
        server.AddDocument(3, content, DocumentStatus::BANNED, rating);
        server.AddDocument(4, content, DocumentStatus::REMOVED, rating);

        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::ACTUAL).size()*1, 1);
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::BANNED).size()*1, 1);
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::IRRELEVANT).size()*1, 1);
        ASSERT_EQUAL(server.FindTopDocuments(query, DocumentStatus::REMOVED).size()*1, 1);
    }
}

void TestRelevance() {
    const vector<int> rating = {1, 2, 3};
    const string content1 = "cat Saint-Petersburg"s;
    const string content2 = "dog Moscow"s;
    
    // выведем два документа с одинаковой релевантностью
    {
        SearchServer server;
        server.AddDocument(1, content1, DocumentStatus::ACTUAL, rating);
        server.AddDocument(2, content2, DocumentStatus::ACTUAL, rating);
        vector<Document> docs = server.FindTopDocuments("cat Moscow"s);
        const double relevance1 = docs[0].relevance;
        const double relevance2 = docs[1].relevance;
        const double EPSILON = 1e-6; // используется для погрешности, т.к. сравниваем вещественные числа
        ASSERT_HINT(abs(relevance1-relevance2) < EPSILON, "document's relevance must be relevance1 ≈ relevance2");
    }

    //проверим, что релевантность одного документа выше, чем другого
    {
        SearchServer server;
        server.AddDocument(1, content1, DocumentStatus::ACTUAL, rating);
        server.AddDocument(2, content2, DocumentStatus::ACTUAL, rating);
        vector<Document> docs = server.FindTopDocuments("cat dog Moscow"s);
        const double relevance1 = docs[0].relevance; // тут релевантность выше, т.к. пересекаются два слова "dog, Moscow"
        const double relevance2 = docs[1].relevance; // тут пересечение с одним словом "cat"
        ASSERT(relevance1 > relevance2);
    }

    {
        SearchServer server;
        server.AddDocument(1, content1, DocumentStatus::ACTUAL, rating);
        server.AddDocument(2, content2, DocumentStatus::ACTUAL, rating);
        Document doc_with_word_cat = server.FindTopDocuments("cat"s)[0];
        const double relevance = doc_with_word_cat.relevance;
        ASSERT(relevance > 0.3 && relevance < 0.4); // relevance ≈ 0.34
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocumentsAndFindDocument);
    RUN_TEST(TestMinusWordsInQueryExludeDocumentFromResult);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestRatingIsEqualArithmeticMeanOfTheDocumentsRatings);
    RUN_TEST(TestPredicatFiltrating);
    RUN_TEST(TestFiltratingByStatus);
    RUN_TEST(TestRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}