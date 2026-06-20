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
#include "forecast.h"
#include "timeseries-regression-data.h"
#include <cassert>

using namespace std;
using namespace std::filesystem;
using namespace timeseries::classifiers;
using namespace timeseries::dataframe;
using namespace timeseries::features;
using namespace timeseries::regression::data;
using namespace forecast;

namespace reports {
    using BinaryClassifierProducer = std::function<std::unique_ptr<BinaryClassifier>(const RegressionData &)>;
    
    BinaryConfusionMatrix confusion_matrix(const RegressionData & regression_data, const BinaryClassifierProducer& binary_classifier_factory) {
        auto binary_classifier = binary_classifier_factory(regression_data);
        auto confusion_matrix_result = binary_classifier->confusion_matrix(regression_data.X_test(), regression_data.Y_test());
        return confusion_matrix_result.value();
    }

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

            auto regression_zscore_data_result = RegressionData::CreateWithZScore(df);
            assert(regression_zscore_data_result.has_value());
            auto & regression_zscore_data = regression_zscore_data_result.value();
        
            auto regression_data_pct_change_result = RegressionData::CreateWithPctChange(df);
            assert(regression_data_pct_change_result.has_value());
            auto & regression_data_pct_change = regression_data_pct_change_result.value();

            auto regression_data_log_change_result = RegressionData::CreateWithLogChange(df);
            assert(regression_data_log_change_result.has_value());
            auto & regression_data_log_change = regression_data_log_change_result.value();

            // 2. Compute confusion matrices immediately
            auto logistic_matrix = confusion_matrix(regression_data_pct_change, [](const RegressionData & d) -> std::unique_ptr<BinaryClassifier> { return std::move(LogisticRegression::Create(d.X_train(), d.Y_train()).value()); });
            auto lda_matrix = confusion_matrix(regression_data_log_change, [](const RegressionData & d) -> std::unique_ptr<BinaryClassifier> { return std::move(LinearDiscriminant::Create(d.X_train(), d.Y_train()).value()); });
            auto qda_matrix = confusion_matrix(regression_data_pct_change, [](const RegressionData & d) -> std::unique_ptr<BinaryClassifier> { return std::move(QuadraticDiscriminant::Create(d.X_train(), d.Y_train()).value()); });
            auto rsvm_matrix = confusion_matrix(regression_zscore_data, [](const RegressionData & d) -> std::unique_ptr<BinaryClassifier> { return std::move(RadialSupportVectorMachine::Create(d.X_train(), d.Y_train(), 1.0/d.X_train().cols(), 1).value());});
            auto random_matrix = confusion_matrix(regression_zscore_data, [](const RegressionData & d) -> std::unique_ptr<BinaryClassifier> { return std::move(RandomForest::Create(d.X_train(), d.Y_train()).value());});

            // 3. Create the report using the confusion matrices (not the unique_ptr<Classifier>)
            return make_unique<QualityReport>(full_file_path, logistic_matrix, lda_matrix, qda_matrix, rsvm_matrix, random_matrix);
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