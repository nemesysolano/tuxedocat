    ```c++
    void position_creation_test(const char * program_directory_){
        // Creates `Broker`
        Broker broker

        // Creates `DataFrameFeed`
        const string program_directory(program_directory_);
        const vector<string> data_file_paths_(data_file_paths(program_directory));
        auto market_event_handler_test_impl =  [](
            const MarketEvent & market_event, 
            const DataFrameFeed & dataframe_feed, 
            const unordered_map<string, size_t> & records_loaded) {
            
        };
    
        auto dataframe_feed_result = DataFrameFeed::Create(data_file_paths_, market_event_handler_test_impl);
        assert(dataframe_feed_result.has_value());
        auto & dataframe_feed = dataframe_feed_result.value();


        log_trace_with_message("PASSED");
    }
    ```