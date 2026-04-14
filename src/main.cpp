#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <random>
#include <cctype>
#include <thread>
#include <chrono>

using namespace std;

#define PASSWORD_FILE "../passwords/passwords.txt"
#define MASTER_KEY "__MASTER_PASSWORD__"
#define SEED_KEY "__ENCODING_SEED__"

static string g_encoding_seed;

bool write_to_file(const string& file_name, const string& key, const string& value);
bool delete_from_file(const string& file_name, const string& key);
string read_from_file(const string& file_name, const string& key);
string encode(const string& input);
string decode(const string& input);
void delete_key();
bool ensure_encoding_seed();
bool ensure_master_password();
bool authenticate_user();

static bool is_reserved_key(const string& key) {
    return key == MASTER_KEY || key == SEED_KEY;
}

static bool is_valid_seed(const string& seed) {
    if (seed.size() != 32) {
        return false;
    }
    for (unsigned char c : seed) {
        if (!std::isalnum(c)) {
            return false;
        }
    }
    return true;
}

static bool parse_key(const string& line, string& out_key) {
    const string key_prefix = "key=\"";
    size_t key_start = line.find(key_prefix);
    if (key_start == string::npos) {
        return false;
    }
    key_start += key_prefix.size();
    size_t key_end = line.find('"', key_start);
    if (key_end == string::npos) {
        return false;
    }
    out_key = line.substr(key_start, key_end - key_start);
    return true;
}

static string build_line(const string& key, const string& value) {
    return "key=\"" + key + "\" value=\"" + value + "\"";
}

static string build_line_with_values(const string& key, const vector<string>& encoded_values) {
    string line = "key=\"" + key + "\"";
    for (size_t i = 0; i < encoded_values.size(); ++i) {
        line += " value_" + to_string(i + 1) + "=\"" + encoded_values[i] + "\"";
    }
    return line;
}

static vector<string> split_by_spaces(const string& input) {
    vector<string> parts;
    istringstream iss(input);
    string token;
    while (iss >> token) {
        parts.push_back(token);
    }
    return parts;
}

static int seed_char_to_shift(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'Z') {
        return 36 + (c - 'A');
    }
    return 3;
}

static string apply_seed_shift(const string& input, bool encode_mode) {
    string output = input;
    for (size_t i = 0; i < output.size(); ++i) {
        int shift = seed_char_to_shift(g_encoding_seed[i % g_encoding_seed.size()]) % 95;
        unsigned char uc = static_cast<unsigned char>(output[i]);
        if (uc >= 32 && uc <= 126) {
            int normalized = static_cast<int>(uc) - 32;
            int shifted = encode_mode
                ? (normalized + shift) % 95
                : (normalized - shift + 95) % 95;
            output[i] = static_cast<char>(shifted + 32);
        }
    }
    return output;
}

static string to_hex(const string& input) {
    static const char* hex_digits = "0123456789abcdef";
    string output;
    output.reserve(input.size() * 2);
    for (unsigned char byte : input) {
        output.push_back(hex_digits[(byte >> 4) & 0x0F]);
        output.push_back(hex_digits[byte & 0x0F]);
    }
    return output;
}

static bool from_hex(const string& input, string& out) {
    if (input.empty() || (input.size() % 2) != 0) {
        return false;
    }
    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };
    out.clear();
    out.reserve(input.size() / 2);
    for (size_t i = 0; i < input.size(); i += 2) {
        int hi = hex_value(input[i]);
        int lo = hex_value(input[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

static string generate_random_seed(size_t length) {
    const string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<size_t> dist(0, charset.size() - 1);
    string seed;
    seed.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        seed.push_back(charset[dist(gen)]);
    }
    return seed;
}

static string trim_copy(const string& input) {
    const string whitespace = " \t\n\r\f\v";
    size_t start = input.find_first_not_of(whitespace);
    if (start == string::npos) {
        return "";
    }
    size_t end = input.find_last_not_of(whitespace);
    return input.substr(start, end - start + 1);
}

static vector<string> extract_values_from_line(const string& line) {
    vector<pair<int, string>> indexed_values;
    size_t pos = 0;
    const string indexed_prefix = " value_";
    while (true) {
        size_t marker = line.find(indexed_prefix, pos);
        if (marker == string::npos) {
            break;
        }
        size_t idx_start = marker + indexed_prefix.size();
        size_t eq_pos = line.find('=', idx_start);
        if (eq_pos == string::npos) {
            break;
        }
        string idx_str = line.substr(idx_start, eq_pos - idx_start);
        if (idx_str.empty()) {
            pos = eq_pos + 1;
            continue;
        }
        bool valid_number = true;
        for (char ch : idx_str) {
            if (ch < '0' || ch > '9') {
                valid_number = false;
                break;
            }
        }
        if (!valid_number) {
            pos = eq_pos + 1;
            continue;
        }
        size_t quote_start = line.find('"', eq_pos);
        if (quote_start == string::npos) {
            break;
        }
        size_t quote_end = line.find('"', quote_start + 1);
        if (quote_end == string::npos) {
            break;
        }
        int idx = stoi(idx_str);
        string value = line.substr(quote_start + 1, quote_end - quote_start - 1);
        indexed_values.push_back({idx, value});
        pos = quote_end + 1;
    }
    if (!indexed_values.empty()) {
        sort(indexed_values.begin(), indexed_values.end(), [](const pair<int, string>& a, const pair<int, string>& b) {
            return a.first < b.first;
        });
        vector<string> values;
        for (const auto& item : indexed_values) {
            values.push_back(item.second);
        }
        return values;
    }
    const string single_prefix = " value=\"";
    size_t value_start = line.find(single_prefix);
    if (value_start == string::npos) {
        return {};
    }
    value_start += single_prefix.size();
    size_t value_end = line.find('"', value_start);
    if (value_end == string::npos) {
        return {};
    }
    return {line.substr(value_start, value_end - value_start)};
}

static bool read_encoded_values_for_key(const string& file_name, const string& key, vector<string>& out_values) {
    ifstream in_file(file_name);
    if (!in_file.is_open()) {
        return false;
    }
    string line;
    while (getline(in_file, line)) {
        string current_key;
        if (!parse_key(line, current_key)) {
            continue;
        }
        if (current_key == key) {
            out_values = extract_values_from_line(line);
            return !out_values.empty();
        }
    }
    return false;
}

static bool is_valid_encoded_value(const string& value) {
    if (value.empty() || (value.size() % 2) != 0) {
        return false;
    }
    for (unsigned char c : value) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    return true;
}

void display_all_keys() {
    ifstream file(PASSWORD_FILE);
    if (!file.is_open()) {
        cout << endl;
        cout << "[ERROR] The file could not be opened." << endl;
        return;
    }
    cout << endl;
    cout << "Stored Keys:" << endl;
    string line;
    bool found_any = false;
    int index = 1;
    while (getline(file, line)) {
        string key;
        if (parse_key(line, key)) {
            if (!is_reserved_key(key)) {
                cout << " " << index++ << ". " << key << endl;
                found_any = true;
            }
        }
    }
    file.close();
    if (!found_any) {
        cout << "[ERROR] No stored keys found." << endl;
    }
}

void search_value() {
    cout << endl;
    cout << "Enter the key to search for: ";
    string key;
    getline(cin, key);
    key = trim_copy(key);
    if (is_reserved_key(key)) {
        cout << "[ERROR] Reserved key." << endl;
        return;
    }
    vector<string> encoded_values;
    if (!read_encoded_values_for_key(PASSWORD_FILE, key, encoded_values)) {
        cout << "[ERROR] Key not found: " << key << endl;
    } else {
        string decoded_values = read_from_file(PASSWORD_FILE, key);
        string encoded_joined;
        for (size_t i = 0; i < encoded_values.size(); ++i) {
            if (i > 0) {
                encoded_joined += " ";
            }
            encoded_joined += encoded_values[i];
        }
        cout << " >> Key: " << key << endl;
        cout << "    Values (encoded): " << encoded_joined << endl;
        cout << "    Values (decoded): " << decoded_values << endl;
    }
}

void add_or_modify_value() {
    cout << endl;
    cout << "Enter the key: ";
    string key;
    getline(cin, key);
    key = trim_copy(key);
    if (key.empty()) {
        cout << "[ERROR] Key cannot be empty." << endl;
        return;
    }
    if (is_reserved_key(key)) {
        cout << "[ERROR] Reserved key." << endl;
        return;
    }
    cout << "Enter values separated by spaces: ";
    string value_line;
    getline(cin, value_line);
    if (value_line.empty()) {
        cout << "[ERROR] You must enter at least one value." << endl;
        return;
    }
    if (write_to_file(PASSWORD_FILE, key, value_line)) {
        vector<string> values = split_by_spaces(value_line);
        string encoded_values;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                encoded_values += " ";
            }
            encoded_values += encode(values[i]);
        }
        cout << "Value saved successfully." << endl;
        cout << " >> Key: " << key << endl;
        cout << "    Values (decoded): " << value_line << endl;
        cout << "    Values (encoded): " << encoded_values << endl;
    } else {
        cout << "[ERROR] Problem saving the value." << endl;
    }
}

bool delete_from_file(const string& file_name, const string& key) {
    vector<string> lines;
    ifstream in_file(file_name);
    if (in_file.is_open()) {
        string line;
        while (getline(in_file, line)) {
            lines.push_back(line);
        }
        in_file.close();
    }
    bool found = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        string current_key;
        if (parse_key(lines[i], current_key) && current_key == key) {
            lines.erase(lines.begin() + i);
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    ofstream out_file(file_name, ios::trunc);
    if (!out_file.is_open()) {
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        out_file << lines[i];
        if (i + 1 < lines.size()) {
            out_file << '\n';
        }
    }
    return true;
}

void delete_key() {
    cout << endl;
    cout << "Enter the key to delete: ";
    string key;
    getline(cin, key);
    key = trim_copy(key);
    if (is_reserved_key(key)) {
        cout << "[ERROR] Reserved key." << endl;
        return;
    }
    if (delete_from_file(PASSWORD_FILE, key)) {
        cout << endl;
        cout << "Key entry deleted successfully. Key: " << key << endl;
    } else {
        cout << "[ERROR] Key not found or could not be deleted. Key: " << key << endl;
    }
}

bool write_to_file(const string& file_name, const string& key, const string& value) {
    vector<string> lines;
    ifstream in_file(file_name);
    if (in_file.is_open()) {
        string line;
        while (getline(in_file, line)) {
            lines.push_back(line);
        }
        in_file.close();
    }
    string new_line;
    if (is_reserved_key(key)) {
        new_line = build_line(key, value);
    } else {
        vector<string> decoded_values = split_by_spaces(value);
        if (decoded_values.empty()) {
            return false;
        }

        vector<string> encoded_values;
        for (const string& part : decoded_values) {
            encoded_values.push_back(encode(part));
        }
        new_line = build_line_with_values(key, encoded_values);
    }
    bool found = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        string current_key;
        if (parse_key(lines[i], current_key) && current_key == key) {
            lines[i] = new_line;
            found = true;
            break;
        }
    }
    if (!found) {
        lines.push_back(new_line);
    }
    ofstream out_file(file_name, ios::trunc);
    if (!out_file.is_open()) {
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        out_file << lines[i];
        if (i + 1 < lines.size()) {
            out_file << '\n';
        }
    }
    return true;
}

string read_from_file(const string& file_name, const string& key) {
    vector<string> encoded_values;
    if (!read_encoded_values_for_key(file_name, key, encoded_values)) {
        return "";
    }
    if (is_reserved_key(key)) {
        return encoded_values[0];
    }
    string decoded_joined;
    for (size_t i = 0; i < encoded_values.size(); ++i) {
        if (i > 0) {
            decoded_joined += " ";
        }
        decoded_joined += decode(encoded_values[i]);
    }
    return decoded_joined;
}

string encode(const string& input) {
    return to_hex(apply_seed_shift(input, true));
}

string decode(const string& input) {
    string shifted;
    if (!from_hex(input, shifted)) {
        return "";
    }
    return apply_seed_shift(shifted, false);
}

bool ensure_encoding_seed() {
    string stored_seed = read_from_file(PASSWORD_FILE, SEED_KEY);
    if (is_valid_seed(stored_seed)) {
        g_encoding_seed = stored_seed;
        return true;
    }
    g_encoding_seed = generate_random_seed(32);
    if (!write_to_file(PASSWORD_FILE, SEED_KEY, g_encoding_seed)) {
        cout << "[ERROR] Could not save encoding seed." << endl;
        return false;
    }

    return true;
}

bool ensure_master_password() {
    string stored_master = read_from_file(PASSWORD_FILE, MASTER_KEY);
    if (is_valid_encoded_value(stored_master)) {
        return true;
    }
    cout << endl;
    cout << "No master password found." << endl;
    cout << "Create a new master password: ";
    string first_input;
    getline(cin, first_input);
    cout << "Confirm master password: ";
    string second_input;
    getline(cin, second_input);
    if (first_input.empty()) {
        cout << "[ERROR] Master password cannot be empty." << endl;
        return false;
    }
    if (first_input != second_input) {
        cout << "[ERROR] Password confirmation does not match." << endl;
        return false;
    }
    if (!write_to_file(PASSWORD_FILE, MASTER_KEY, encode(first_input))) {
        cout << "[ERROR] Could not save master password." << endl;
        return false;
    }
    cout << "Master password created successfully." << endl;
    return true;
}

bool authenticate_user() {
    string stored_master = read_from_file(PASSWORD_FILE, MASTER_KEY);
    cout << endl;
    if (stored_master.empty()) {
        cout << "[ERROR] Master password not configured." << endl;
        return false;
    }
    const int max_attempts = 3;
    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        cout << "Enter master password: ";
        string input;
        getline(cin, input);
        if (encode(input) == stored_master) {
            cout << "Access granted." << endl;
            return true;
        }
        cout << "[ERROR] Invalid password (" << attempt << "/" << max_attempts << ")." << endl;
    }
    cout << "Maximum attempts reached. Exiting..." << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return false;
}

void show_menu() {
    cout << endl;
    cout << " 1. Show all stored keys" << endl;
    cout << " 2. Search for a value by key" << endl;
    cout << " 3. Add or modify a value" << endl;
    cout << " 4. Delete a key entry" << endl;
    cout << " 5. Exit" << endl;
    cout << "Choose an option (1-5): ";
}

int main() {
    if (!ensure_encoding_seed()) {
        return 1;
    }
    if (!ensure_master_password()) {
        return 1;
    }
    if (!authenticate_user()) {
        return 1;
    }
    string option;
    bool running = true;
    while (running) {
        show_menu();
        getline(cin, option);
        if (option == "1") {
            display_all_keys();
        } else if (option == "2") {
            search_value();
        } else if (option == "3") {
            add_or_modify_value();
        } else if (option == "4") {
            delete_key();
        } else if (option == "5") {
            cout << "Closing the application..." << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            running = false;
        } else {
            cout << "[ERROR] Invalid option." << endl;
        }
    }
    return 0;
}
