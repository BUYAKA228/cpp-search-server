#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

//#pragma GCC diagnostic ignored "-Wunused-parameter"
void TestSearchServer();

#define TEST true

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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        auto freq = 1.0 / words.size();
        for (auto& word : words) {
            word_to_document_freqs_[word][document_id] += freq;
        }
        documents_.emplace(document_id, DocumentData { ComputeAverageRating(ratings), status });
        ++document_count_;
    }

    template<typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate predicate) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                const double EPSILON = 1e-6;
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                return lhs.relevance > rhs.relevance; 
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status= DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query,
            [&status](int document_id, DocumentStatus new_status, int rating) {
                return new_status == status;
            });
    }


    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query_words = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query_words.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query_words.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    map<string, map<int, double>> word_to_document_freqs_;

    map<int, DocumentData> documents_;

    int document_count_ = 0;

    set<string> stop_words_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
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

    Query ParseQuery(const string& text) const {
        Query query;
        for (string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                word = word.substr(1);
                if (!IsStopWord(word)) {
                    query.minus_words.insert(word);
                }
            }
            query.plus_words.insert(word);
        }
        return query;
    }

    double ComputeIDF(int found_in_docs) const 
    { 
        return log(static_cast<double>(document_count_)/found_in_docs); 
    } 

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query_words, DocumentPredicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query_words.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            auto tempFreq =  word_to_document_freqs_.at(word);
            double IDF = ComputeIDF(tempFreq.size());
            for (const auto& [document_id, term_freq] : tempFreq) {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += IDF * term_freq;
                }
            }
        }
        for (const string& word : query_words.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() {
    #if TEST == true
    TestSearchServer();
    #endif

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

#if TEST == true
// -------- Начало модульных тестов поисковой системы ----------
template <typename T, typename U>
ostream& operator<<(ostream& out, const pair<T, U>& container) {
    return out << container.first << ": " << container.second;
}

template <typename T>
void Print(ostream& out, const T& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (is_first) {
            out << element;
            is_first = false;
        }
        else {
            out << ", "s << element;
        }
    }
}

template <typename T>
ostream& operator<<(ostream& out, const vector<T>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename T>
ostream& operator<<(ostream& out, const set<T>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
ostream& operator<<(ostream& out, const map<T, U>& container) {
    out << "{"s;
    Print(out, container);
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

template <typename T>
void RunTestImpl(T func, const string& func_str) {
    func();
    cerr << func_str << " OK" << endl;
}


#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func) RunTestImpl((func), #func)

void MatchDocumentAlwaysReturnActualStatus() {
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3, doc_id_4 = 4;
    const string content_1 = "cat the city"s, content_2 = " white cat in box"s, content_3 = "dog sleep in box"s, content_4 = "hello world";
    const vector<int> ratings_1 = {1, 2, 3};
    const vector<int> ratings_2 = {2, 3, 8};
    const vector<int> ratings_3 = {-6, 3, 0};
    const vector<int> ratings_4 = {-6, 3, 0};
    {
        SearchServer server;        
        server.AddDocument(doc_id_1, content_1, DocumentStatus::BANNED, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::IRRELEVANT, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        server.AddDocument(doc_id_4, content_4, DocumentStatus::ACTUAL, ratings_4);
        const auto result_1 = server.MatchDocument("cat",1);    
        ASSERT_EQUAL_HINT(get<DocumentStatus>(result_1) == DocumentStatus::BANNED,true,"not good"s);
        const auto result_2 = server.MatchDocument("cat",2);    
        ASSERT_EQUAL_HINT(get<DocumentStatus>(result_2) == DocumentStatus::IRRELEVANT,true,"not good"s);
        const auto result_3 = server.MatchDocument("cat",3);    
        ASSERT_EQUAL_HINT(get<DocumentStatus>(result_3) == DocumentStatus::ACTUAL,true,"not good"s);
        const auto result_4 = server.MatchDocument("hello -world",4);
        vector<string> as_ht =get<vector<string>>(result_4);
        ASSERT_HINT(as_ht.empty(),"not good"s);
    }
    
}


void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,"not pass");
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("city");
        ASSERT_EQUAL(result.size(),1);
        ASSERT_EQUAL(result[0].id,42);   
    }
}

void MinusWordsAreNotParsed(){
    const int doc_id_1 = 1, doc_id_2 = 2, doc_id_3 = 3;
    const string content_1 = "cat the city"s, content_2 = " white cat in box"s, content_3 = "dog sleep in box"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const vector<int> ratings_2 = {2, 3, 8};
    const vector<int> ratings_3 = {-6, 3, 0};
    {
        SearchServer server;        
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto result_1 = server.FindTopDocuments("box -cat");    
        ASSERT_EQUAL_HINT(result_1.size(),1,"Answer have doc with minusword"s);
        ASSERT_EQUAL_HINT(result_1[0].id,doc_id_3,"Answer have doс with minusword"s);        
    }
}

int AverageRating(const vector<int>& v) {
    int result = 0;    
    for (int i : v) 
    {
       result += i;  
    }
    int av_rat = result/static_cast<int>(v.size());
    return av_rat;
}

void TestRating(){    
    const int doc_id_1 = 10, doc_id_2 = 20, doc_id_3 = 30;
    const string content_1 = "rat in the box"s, content_2 = "white cat in box"s, content_3 = "dog sleep in box"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const vector<int> ratings_2 = {2, 3, 7};
    const vector<int> ratings_3 = {-6, 3, 0};    
    int av_rat_1 = AverageRating(ratings_1);
    int av_rat_2 = AverageRating(ratings_2);
    int av_rat_3 = AverageRating(ratings_3);    
    string query = "box"s;
    {
        SearchServer server;               
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        vector<Document> result = server.FindTopDocuments(query);        
        ASSERT_EQUAL_HINT(result[0].rating,av_rat_2,"Rating 2 wrong"s);           
        ASSERT_EQUAL_HINT(result[1].rating,av_rat_1,"Rating 1 wrong"s);
        ASSERT_EQUAL_HINT(result[2].rating,av_rat_3,"Rating 3 wrong"s);        
    }
    
}

void DocumentPredicateIsIgnored () {    
    const int doc_id_1 = 10, doc_id_2 = 20, doc_id_3 = 30;
    const string content_1 = "rat in the box"s, content_2 = "white cat in box"s, content_3 = "dog sleep in box"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const vector<int> ratings_2 = {2, 3, 7};
    const vector<int> ratings_3 = {-6, 3, 0};    
    DocumentStatus status_1 = DocumentStatus::ACTUAL;
    DocumentStatus status_2 = DocumentStatus::IRRELEVANT;
    DocumentStatus status_3 = DocumentStatus::BANNED;
    string query = "box"s;    
    {
        SearchServer server;               
        server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
        server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
        server.AddDocument(doc_id_3, content_3, status_3, ratings_3);
        vector<Document> result_1 = server.FindTopDocuments(query,DocumentStatus::BANNED);        
        ASSERT_EQUAL_HINT(result_1[0].id,doc_id_3,"Not BANNED"s);  
        vector<Document> result_2 = server.FindTopDocuments(query,DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(result_2[0].id,doc_id_2,"Not IRRELEVANT"s);
        vector<Document> result_3 = server.FindTopDocuments(query,DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(result_3[0].id,doc_id_1,"Not ACTUAL"s);
    }    
}
void OversimplifiedRelevanceCalculation() {
    const int doc_id_1 = 10, doc_id_2 = 20, doc_id_3 = 30;
    const string content_1 = "rat in the box"s, content_2 = "white cat in box"s, content_3 = "dog sleep in box"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const vector<int> ratings_2 = {2, 3, 7};
    const vector<int> ratings_3 = {-6, 3, 0};    
    {
        SearchServer server;               
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        vector<Document> result = server.FindTopDocuments("box"s);        
        ASSERT_EQUAL_HINT(result[0].relevance,log(3/3)*0.25,"Rating 2 wrong"s);           
        ASSERT_EQUAL_HINT(result[1].relevance,log(3/3)*0.25,"Rating 1 wrong"s);
        ASSERT_EQUAL_HINT(result[2].relevance,log(3/3)*0.25,"Rating 3 wrong"s); 
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);    
    RUN_TEST(MinusWordsAreNotParsed);
    RUN_TEST(TestRating);
    RUN_TEST(DocumentPredicateIsIgnored);
    RUN_TEST(MatchDocumentAlwaysReturnActualStatus);
    RUN_TEST(OversimplifiedRelevanceCalculation);
    
}
// --------- Окончание модульных тестов поисковой системы -----------
#endif