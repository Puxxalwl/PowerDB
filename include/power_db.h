#pragma once
#include <memory>
#include <string>
#include <vector>
#include<unordered_map>
#include<atomic>
#include<variant>
#include <shared_mutex>
#include<future>
#include<queue>
#include<thread>
#include<condition_variable>

#ifdef _WIN32
    #define POWER_DB_EXPORT __declspec(dllexport)
#else
    #define POWER_DB_EXPORT __attribute__((visiblity("default")))
#endif

namespace PowerDB {

enum class DataType {
    BOOLEAN, // bool
    TINYUNT, // 1-byte
    SMALLINT, // 2-byte
    INT, // 4-byte
    LONG, //8-byte
    FLOAT, // 4-byte
    DOUBLE, // 8-byte
    DELICMAL,
    CHAR, //fixing line
    VARCHAR, 
    TEXT, // big text
    BINARY, //binary data
    BLOB, // big BINARY
    TIME,
    DATE,
    DATETIME,
    TIMESTAP,
    UUID,
    JSON,
    POINT,
    POLYGON,
    IMAGE,
    AUDIO,
    VIDEO,
    DOCUMENT
};

enum class Mode {
    SYNC,
    ASYNC,
    PARALLEL,
    ASYNC_PARALLEL
};

class POWER_DB_EXPORT Value {
    private:
        DataType type_;
        std::variant<bool,
        int8_t,
        int16_t,
        int32_t,
        int64_t,
        float,
        double,
        std::string,
        std::vector<uint8_t>> data_;

    public:
        //Constructors
        Value() : type_(DataType::INT), data_(0) {}
        Value(bool value) : type_(DataType::BOOLEAN), data_(value) {}
        Value(int8_t value) : type_(DataType::TINYUNT), data_(value) {}
        Value(int16_t value) : type_(DataType::SMALLINT), data_(value) {}
        Value(int32_t value) : type_(DataType::INT), data_(value) {}
        Value(int64_t value) : type_(DataType::LONG), data_(value) {}
        Value(float value) : type_(DataType::FLOAT), data_(value) {}
        Value(double value) : type_(DataType::DOUBLE), data_(value) {}
        Value(const std::string& value) : type_(DataType::VARCHAR), data_(value) {}
        Value(char* value) : type_(DataType::VARCHAR), data_(std::string(value)) {}
        Value(std::vector<uint8_t> value) : type_(DataType::BLOB), data_(value) {}

        // access methods
        DataType type() const { return type_; }

        template<typename T>
        T as() const {return std::get<T>(data_)}
        std::string to_string() const;
        std::vector<uint8_t> serialize() const;
        static Value deserialize(const std::vector<uint8_t>& data);
        size_t memory_size() const;
};

struct POWER_DB_EXPORT Column {
    std::string name;
    DataType type;
    uint32_t length;
    bool nullable;
    bool primary_key;
    std::string default_value;

    Column(const std::string& n, DataType t, uint32_t len = 0, bool null = true, bool pk = false, const std::string& def = "") : name(n), type(t), length(len), nullable(null), primary_key(pk),default_value(def){}
};

class POWER_DB_EXPORT TableSchema {
    private: 
        std::string name_;
        std::vector<Column> colummns_;
        std::unordered_map<std::string, size_t> column_id_;
        std::vector<std::string> primary_key_;
    public:
        TableSchema(const std::string& name) : name_(name){}

        void add_column(Column& column) {
            colummns_.push_back(column);
            column_id_[column.name] = colummns_.size() - 1;
            if (column.primary_key) { 
                primary_key_.push_back(column.name);
            }
        }

        const std::string& name() const {return name_;}
        const std::vector<Column>&columns() const{return colummns_;}
        const std::vector<std::string>& primary_keys() const {return primary_key_;}

        const Column& get_column(const std::string& name) const { return colummns_[column_id_.at(name)];}
        size_t get_column_id (const std::string& name) const {return column_id_.at(name);}
};

class POWER_DB_EXPORT QueryResult {
    public:
        bool success;
        std::string error_msg;
        std::vector<std::string> columns;
        std::vector<std::vector<Value>> rows;
        uint64_t affected_rows;
        uint64_t execution_time_ms;

        QueryResult() : success(true), affected_rows(0), execution_time_ms(0) {}

        size_t row_count()const {return rows.size();}
        size_t column_count()const {return columns.size();}
        const std::vector<Value>& operator[](size_t id) {return rows[id];}
};

struct POWER_DB_EXPORT QueryOptions {
    Mode mode= Mode::SYNC;
    uint32_t parallelism = 0;
    std::string priority = "normal";
    uint32_t timeout_ms = 0;
    bool use_cache = true;
    bool user_vectorisation = true;

    QueryOptions() = default;

    QueryOptions& async() { mode = Mode::ASYNC; return *this; }
    QueryOptions& parallel(uint32_t lvl = 0) {
        if (mode == Mode::ASYNC) {mode = Mode::ASYNC_PARALLEL;} else {mode= Mode::PARALLEL;}
        parallelism = lvl;
        return *this;
    }
    QueryOptions& with_timeout(uint32_t tmo) { timeout_ms = tmo; return *this; } // tmo = timeout
    QueryOptions& with_priority(std::string prio) { priority = prio; return *this; }
};

class POWER_DB_EXPORT Database { 
    private:
        class impl;
        std::unique_ptr<impl> impl_;

    public: 
        Database();
        ~Database();

        // control connect
        bool open(std::string path = ":memory:");
        bool close();
        bool is_open() const;

        // control table
        bool create_table(const TableSchema&schema);
        bool drop_table(const std::string& name);
        bool alter_table(const std::string& name, const std::string& opr); // opr = operation

        // executions sql
        QueryResult execute_query(const std::string& name, const QueryOptions& opt = {});
        QueryResult execute_query(const std::string& name, std::vector<Value>& params,const QueryOptions& opt = {});

        // async executions sql
        std::future<QueryResult> execute_query_async(const std::string& name, const QueryOptions& opt = {});
        std::future<QueryResult>execute_query_async(const std::string& name, std::vector<Value>& params,const QueryOptions& opt = {});

        // batch operations
        bool begin_transaction();
        bool commit_transaction();
        bool rollback_transaction();

        // control id
        bool create_id(const std::string& table_name, const std::vector<std::string>& columns, const std::string& id_type= "btree"); // index
        bool drop_id(const std::string id_name);

        // direct operation
        uint64_t insert(const std::string& table_name, const std::unordered_map<std::string, Value>& values) const;
        int update(const std::string& table_name, const std::unordered_map<std::string, Value>& values, const std::string& where_clause);
        int delete_rows(const std::string& table_name, const std::string& where_clause);

        // info
        std::vector<std::string>get_table_names() const;
        TableSchema get_table_shema(const std::string& table_name) const;
        std::unordered_map<std::string, std::string> get_database_info() const;

        // utils
        void vacuum();
        void analyze();
        void backup(const std::string& filename);
        void restore(const std::string& filename);
};

class POWER_DB_EXPORT QueryBuilder {
    private:
        Database& db_;
        std::string table_name_;
        std::vector<std::string> columns_;
        std::string where_clause_;
        std::vector<std::string> group_by_;
        std::string having_clause_;
        std::vector<std::pair<std::string, bool>> order_by_;
        uint64_t limit_;
        uint64_t offset_;
        QueryOptions options_;
    
    public:
        QueryBuilder(Database& db, const std::string& table_name) : db_(db), table_name_(table_name), limit_(0) , offset_(0) {}
        
        // SELECT
        QueryBuilder& select(const std::vector<std::string>& columns) { columns_ = columns; return *this;}
        QueryBuilder& select(const std::string& column) {columns_ = {column}; return *this;}
        QueryBuilder& select_all() {columns_ = {"*"}; return *this;}
        
        // WHERE
        QueryBuilder& where(const std::string& cond) {where_clause_ = cond; return *this;} // condition
        
        // GROUP BY
        QueryBuilder& group_by(const std::vector<std::string>& columns) {group_by_ = columns; return *this;}
        
        // HAVING
        QueryBuilder& having(const std::string& cond) {having_clause_ = cond; return *this;} // condition
        
        // ORDER BY
        QueryBuilder& order_by(const std::string& column, bool ascending = true) {order_by_.emplace_back(column, ascending); return *this;}
        
        // LIMIT/OFFSET
        QueryBuilder& limit(uint64_t count) {limit_ = count; return *this;}
        QueryBuilder& offset(uint64_t count) {offset_ = count; return *this;}
        
        // options exection
        QueryBuilder& async() {options_.async(); return *this;}
        QueryBuilder& parallel(uint32_t lvl = 0) {options_.parallel(lvl); return *this;}
        
        QueryBuilder& with_timeout(uint32_t tom) {options_.with_timeout(tom); return *this;} // timeout_ms
        
        // exection
        QueryResult execute() { std::string sql = build_sql(); return db_.execute_query(sql, options_);}
        std::future<QueryResult> execute_async() {std::string sql = build_sql(); return db_.execute_query_async(sql, options_);}
        
    private:
        std::string build_sql() const {
            std::string sql = "SELECT ";
            
            // column
            if (columns_.empty() || (columns_.size() == 1 && columns_[0] == "*")) {sql += "*";} else {
                for (size_t i = 0; i < columns_.size(); ++i) {
                    if (i > 0) sql += ", ";
                    sql += columns_[i];
                }
            }
            
            sql += " FROM " + table_name_;
            
            // WHERE
            if (!where_clause_.empty()){sql += " WHERE " + where_clause_;}
            
            // GROUP BY
            if (!group_by_.empty()){
                sql += " GROUP BY ";
                for (size_t i = 0; i < group_by_.size(); ++i) {
                    if (i > 0) sql += ", ";
                    sql += group_by_[i];
                }
            }
            
            // HAVING
            if (!having_clause_.empty()){sql += " HAVING " + having_clause_;}
            
            // ORDER BY
            if (!order_by_.empty()){
                sql += " ORDER BY ";
                for (size_t i = 0; i < order_by_.size(); ++i) {
                    if (i > 0) sql += ", ";
                    sql += order_by_[i].first;
                    sql += order_by_[i].second ? " ASC" : " DESC";
                }
            }
            
            // LIMIT/OFFSET
            if (limit_ > 0){sql += " LIMIT " + std::to_string(limit_);}
            if (offset_ > 0) {sql += " OFFSET " + std::to_string(offset_);}
            
            return sql;
        }
};

extern "C" {
    POWER_DB_EXPORT void* power_db_open(const char* path);
    POWER_DB_EXPORT void power_db_close(void* db);
    POWER_DB_EXPORT int power_db_execute(void* db, const char* sql, void* result_out);
    POWER_DB_EXPORT int power_db_execute_async(void* db, const char* sql, void* callback, void* user_data);
    POWER_DB_EXPORT int power_db_create_table(void* db, const char* name, const char* schema_json);
    POWER_DB_EXPORT const char* power_db_last_error(void* db);
    POWER_DB_EXPORT void power_db_free_result(void* result);
}

} // namespace PowerDB