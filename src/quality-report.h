#ifndef __QUALITY_REPORT_H__
#define __QUALITY_REPORT_H__
#include <string>
#include <vector>
#include "timeseries-classifiers.h"
#include "memory"
using namespace std;
using namespace timeseries::classifiers;

#define __QUALITY_REPORT_H__
namespace reports {
    class QualityReport {
        private:
            string file_path_;
            BinaryConfusionMatrix logistic_;
            BinaryConfusionMatrix linear_discriminant_;
            BinaryConfusionMatrix quadratic_discriminant_;

        public:
            QualityReport(const string & file_path, BinaryConfusionMatrix logistic, BinaryConfusionMatrix linear_discriminant, BinaryConfusionMatrix quadratic_discriminant): 
            file_path_(file_path), logistic_(logistic), linear_discriminant_(linear_discriminant), quadratic_discriminant_(quadratic_discriminant) {}
            const string & file_path() const { return file_path_; }
            const BinaryConfusionMatrix & logistic() const { return logistic_; }
            const BinaryConfusionMatrix & linear_discriminant() const { return linear_discriminant_; }
            const BinaryConfusionMatrix & quadratic_discriminant() const { return quadratic_discriminant_; }
    };

    unique_ptr<QualityReport> quality(const string & full_file_path);
    void quality(const char * current_program_path, const vector<string> & files);
}
#endif
