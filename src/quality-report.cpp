#include "quality-report.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iterator>
#include "timeseries-classifiers.h"
#include "timeseries-dataframe.h"
#include "timeseries-features.h"
#include <iostream>
#include <algorithm>
#include <string_view>
#include <ranges>
#include <execution>
#include <future>

using namespace std;
using namespace std::filesystem;
using namespace timeseries::classifiers;
using namespace timeseries::dataframe;
using namespace timeseries::features;

namespace reports {
    unique_ptr<QualityReport> quality(const string & full_file_path) {
        try {
            auto input_stream = ifstream(full_file_path);    
            auto dataframe_result = DataFrame::Create(input_stream);

            if(!dataframe_result) {
                cout << "Can't load " << full_file_path << endl;            
                return nullptr;            
            }
            auto & df = dataframe_result.value();
            input_stream.close();

            auto momenta_result = Features::CreateWithZScore(df);
            if(!momenta_result.has_value()) {
                return nullptr;
            }
            auto & momenta = momenta_result.value();
            const DataFrame & momentum_df = momenta.data_frame();
            const std::vector<std::string> & momentum_column_names = momenta.momentum_column_names();
            size_t train_end_row = momentum_df.rows() * 0.8;

            auto X_result = momentum_df.copy(momentum_column_names, momentum_column_names);
            auto & X = X_result.value();

            auto X_train_result = X.slice_to(train_end_row);
            auto & X_train = X_train_result.value();

            auto X_test_result = X.slice_from(train_end_row);
            auto & X_test = X_test_result.value();    

            auto Y_result = momentum_df.copy({"Direction"}, {"Direction"});
            auto & Y = Y_result.value();;

            auto Y_train_result = Y.slice_to(train_end_row);
            auto & Y_train = Y_train_result.value();

            auto Y_test_result = Y.slice_from(train_end_row);
            auto & Y_test = Y_test_result.value();
            
            // 1. Create the classifiers
            auto logistic_result = LogisticRegression::Create(X_train, Y_train);
            auto lda_result = LinearDiscriminant::Create(X_train, Y_train);
            auto qda_result = QuadraticDiscriminant::Create(X_train, Y_train);
            auto rsvm_result = RadialSupportVectorMachine::Create(X_train, Y_train, 1 / static_cast<double>(X_train.cols()) , 1);
            auto random_forex_result = RandomForest::Create(X_train, Y_train);

            // 2. Compute confusion matrices immediately
            auto logistic_matrix = logistic_result.value()->confusion_matrix(X_test, Y_test).value();
            auto lda_matrix = lda_result.value()->confusion_matrix(X_test, Y_test).value();
            auto qda_matrix = qda_result.value()->confusion_matrix(X_test, Y_test).value();
            auto rsvm_matrix = rsvm_result.value()->confusion_matrix(X_test, Y_test).value();
            auto random_forex = random_forex_result.value()->confusion_matrix(X_test, Y_test).value();

            // 3. Create the report using the confusion matrices (not the unique_ptr<Classifier>)
            return make_unique<QualityReport>(full_file_path, logistic_matrix, lda_matrix, qda_matrix, rsvm_matrix, random_forex);
        } catch (const exception & e) {
            cout << "failed to process " << full_file_path << endl;
            return nullptr;
        }
    
    }

void quality(const vector<string> & files) {        
    std::vector<std::future<std::unique_ptr<QualityReport>>> futures;
    futures.reserve(files.size());

   //  1. Dispatch all tasks asynchronously 
    for (const auto& file : files) {
        futures.push_back(std::async(std::launch::async, [&file]() {
            return quality(file);
        }));
    }
    
    // 2. Gather results, blocking until each thread finishes, and filter
    std::vector<std::unique_ptr<QualityReport>> reports;
    for (auto& fut : futures) {
        auto report = fut.get();
        if (report != nullptr) {
            reports.push_back(std::move(report));
        }
    }

        // --- NEW: Tabulated Output Engine ---
        
        // 1. Print Exact ASCII Headers
        cout << "                                                                                                             CLASSIFIERS REPORT" << endl;
        cout << "|------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+" << endl;
        cout << "|Ticker|                                 Logistic                                            |                                 Linear Discriminant Analysis                        |                                Quadratic Discriminant Analysis                      |                                Rarial Support Vector Machine.                       |                                Random Forex                                         |" << endl;
        cout << "+------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+" << endl;
        cout << "|      |              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|" << endl;
        cout << "|      |----------------------+----------------------+---------+---------+---------+---------+----------------------+----------------------+---------+---------+---------+---------+----------------------+----------------------+---------+---------+---------+---------+----------------------+----------------------+---------+---------+---------+---------+----------------------+----------------------+---------+---------+---------+---------+" << endl;
        cout << "|      |      True|      False|      True|      False|                                       |      True|      False|      True|      False|                                       |      True|      False|      True|      False|                                       |      True|      False|      True|      False|                                       |      True|      False|      True|      False|                                       |" << endl;
        cout << "+------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+" << endl;

        // 2. Lambda to format and print a single classifier's metrics
        auto print_metrics = [](const BinaryConfusionMatrix& m) {
            cout << "|" << std::right << std::setw(10) << m.true_positives()
                 << "|" << std::right << std::setw(11) << m.false_positives()
                 << "|" << std::right << std::setw(10) << m.true_negatives()
                 << "|" << std::right << std::setw(11) << m.false_negatives()
                 << "|" << std::right << std::setw(9) << std::fixed << std::setprecision(7) << m.accuracy()
                 << "|" << std::right << std::setw(9) << std::fixed << std::setprecision(7) << m.precision()
                 << "|" << std::right << std::setw(9) << std::fixed << std::setprecision(7) << m.recall()
                 << "|" << std::right << std::setw(9) << std::fixed << std::setprecision(7) << m.f1_score();
        };

        // 3. Print Data Rows
        for (const auto& report : reports) {
            if (report) {
                // Extract ticker from filename and clamp to 6 characters to prevent layout breaking
                string ticker = std::filesystem::path(report->file_path()).stem().string();
                if (ticker.length() > 6) ticker = ticker.substr(0, 6); 

                // Ticker column
                cout << "|" << std::left << std::setw(6) << ticker;

                // Print Logistic Block
                print_metrics(report->logistic());

                // Print LDA Block
                print_metrics(report->linear_discriminant());
                
                // Print LDA Block
                print_metrics(report->quadratic_discriminant());                
                
                // Print RSVM Block
                print_metrics(report->rsvm());                

                // Print RandomForex Block
                print_metrics(report->random_forex());
                cout << "|" << endl;                
            }
        }
        
        // 4. Print Footer
        cout << "+------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+" << endl;
    }

}
/* 
                                                                                    CLASSIFIERS REPORT
|------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+
|Ticker|                                 Logistic                                            |                                 Linear Discriminant Analysis                        |
+------+-------------------------------------------------------------------------------------+-------------------------------------------------------------------------------------+
|      |              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|              Positive|              Negative| Accuracy|Precision|   Recall| F1_Score|
|      |----------------------+----------------------+---------+---------+---------+---------+----------------------+----------------------+---------+---------+---------+---------+
|      |      True|      False|      True|      False|                                       |      True|      False|      True|      False|                                       |
+------+----------+-----------+----------+-----------+---------+---------+---------+---------+----------+-----------+----------+-----------+---------+---------+---------+---------+
|ABCD  |      1234|       1234|      1234|       1234|#.4808081|#.5555556|#.3610108|#.4376368|      1234|       1234|      1234|       1234|#.4808081|#.5555556|#.3610108|#.4376368|
*/