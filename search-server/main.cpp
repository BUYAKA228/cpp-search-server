#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <optional>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() 
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text)
{
    vector<string> words;
    string word;
    for (const char c : text) 
    {
        if (c == ' ') 
        {
            if (!word.empty()) 
            {
                words.push_back(word);
                word.clear();
            }
        } else 
            word += c;
    }
    if (!word.empty())
        words.push_back(word);
    return words;
}

struct Document 
{
    Document() = default;

    Document(int id_,   double relevance_,      int rating_)
            : id(id_),  relevance(relevance_),  rating(rating_)
    {}

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeStrings(const StringContainer& strings) 
{
    set<string> non_empty_strings;
    for (const string& str : strings) 
        if (!str.empty()) 
            non_empty_strings.insert(str);
    return non_empty_strings;
}

enum class DocumentStatus 
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};


class SearchServer 
{
    public:
        template <typename StringContainer>
        explicit SearchServer(const StringContainer& stop_words)
        {
            if (all_of(stop_words.begin(), stop_words.end(), IsValidWord)) 
                stop_words_ = MakeStrings(stop_words);
            else 
                throw invalid_argument("содержит спец символ"s);
        }

        explicit SearchServer(const string& stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text))
        {
        }

        void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) 
        {
            if (document_id < 0) 
                throw invalid_argument("отрицательный id"s);
            if (documents_.count(document_id))
                throw invalid_argument("повторный id"s);
            
   
            const vector<string> words = SplitIntoWordsNoStop(document);
            auto freq = 1.0 / words.size(); 
            for (auto& word : words) 
                word_to_document_freqs_[word][document_id] += freq; 
            documents_.emplace(document_id, DocumentData { ComputeAverageRating(ratings), status });
            documents_index_.push_back(document_id);
        }

        template <typename DocumentPredicate>
        vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const 
        {
            Query query = ParseQuery(raw_query);
            auto matched_documents = FindAllDocuments(query, document_predicate);
            sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) 
            {
                const double EPSILON = 1e-6; 

                if (abs(lhs.relevance - rhs.relevance) < EPSILON) 
                    return lhs.rating > rhs.rating; 
                return lhs.relevance > rhs.relevance;
            });
            if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) 
                matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
            return matched_documents;
        }

        vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status=DocumentStatus::ACTUAL) const 
        {
            return FindTopDocuments(raw_query,[&status](int document_id, DocumentStatus new_status, int rating) 
            {
                return new_status == status;
            });
        }

        int GetDocumentCount() const {
            return static_cast<int>(documents_.size());
        }

        int GetDocumentId(int index) const {
                return documents_index_.at(index);
        }

        tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const 
        {
            Query query = ParseQuery(raw_query);
            vector<string> matched_words;
            for (const string& word : query.plus_words) 
            {
                if (word_to_document_freqs_.count(word) == 0) 
                    continue;
                if (word_to_document_freqs_.at(word).count(document_id)) 
                    matched_words.push_back(word);
            }
            for (const string& word : query.minus_words) 
            {
                if (word_to_document_freqs_.count(word) == 0) 
                    continue;
                if (word_to_document_freqs_.at(word).count(document_id)) 
                {
                    matched_words.clear();
                    break;
                }
            }
            return { matched_words, documents_.at(document_id).status };
        }

    private:
        struct Query
        {
            set<string> plus_words;
            set<string> minus_words;
        };

        struct DocumentData
        {
            int rating;
            DocumentStatus status;
        };

        map<string, map<int, double>> word_to_document_freqs_;
        map<int, DocumentData> documents_;
        set<string> stop_words_;
        vector<int> documents_index_;

        bool IsStopWord(const string& word) const 
        {
            return stop_words_.count(word) > 0;
        }

        static bool IsValidWord(const string& word) 
        {
            // A valid word must not contain special characters
            return none_of(word.begin(), word.end(), [](char c) 
            {
                return c >= '\0' && c < ' ';
            });
        }

        vector<string> SplitIntoWordsNoStop(const string& text) const
        {
            vector<string> words;
            for (const string& word : SplitIntoWords(text)) 
            {
                if (!IsValidWord( word)) 
                    throw invalid_argument("недопустимые знаки"s);
                if (!IsStopWord(word))
                    words.push_back(word);
            }
            return words;
        }

        static int ComputeAverageRating(const vector<int>& ratings) 
        { 
            if (ratings.empty()) 
                return 0; 
            int rating_sum = accumulate(ratings.begin(), ratings.end(), 0); 
            return rating_sum / static_cast<int>(ratings.size()); 
        } 

        struct QueryWord 
        {
            string data;
            bool is_minus;
            bool is_stop;
        };

        QueryWord ParseQueryWord(string text) const 
        {
            QueryWord result;
            if (text.empty()) 
                throw invalid_argument("в минус слове отсутствует текст"s);
            bool is_minus = false;
            if (text[0] == '-') 
            {
                is_minus = true;
                text = text.substr(1);
            }
            if (text.empty() || text[0] == '-' || !IsValidWord(text)) 
                throw invalid_argument("в минус слове повторный знак минуса"s);
            return { text, is_minus, IsStopWord(text) };
        }

        Query ParseQuery(const string& text) const 
        {
            Query result;
            for (const string& word : SplitIntoWords(text)) 
            {
                QueryWord query_word = ParseQueryWord(word);
                if (!query_word.is_stop) 
                {
                    if (query_word.is_minus) 
                        result.minus_words.insert(query_word.data);
                    else 
                        result.plus_words.insert(query_word.data);
                }
            }
            return result;
        }

        double ComputeIDF(int found_in_docs) const 
        { 
            return log(static_cast<double>(GetDocumentCount() * 1.0)/found_in_docs); 
        } 

        template<typename DocumentPredicate>
        vector<Document> FindAllDocuments(const Query& query, DocumentPredicate predicate) const 
        {
            map<int, double> document_to_relevance;
            for (const string& word : query.plus_words)
            {
                if (word_to_document_freqs_.count(word) == 0) 
                    continue;
                
                const double IDF = ComputeIDF(word_to_document_freqs_.at(word).size());

                for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) 
                {
                    const DocumentData documents_data = documents_.at(document_id);
                    if (predicate(document_id, documents_data.status, documents_data.rating)) 
                        document_to_relevance[document_id] += IDF * term_freq;
                }
            }
            for (const string& word : query.minus_words)
            {
                if (word_to_document_freqs_.count(word) == 0) 
                    continue;
                for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) 
                    document_to_relevance.erase(document_id);
            }

            vector<Document> matched_documents;
            for (const auto &[document_id, relevance] : document_to_relevance) 
                matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
            
            return matched_documents;
        }
};
int main()
{
    std::cout << "hello" <<std::endl;
}
