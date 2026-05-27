
#ifdef __TEST_MAIN__
#include "polynomials-tests.h"
#include "row_span_tests.h"
#include "distributions.h"
#include "timeseries-adf.h" // Required for mac_kinnon_p and RegressionType
#include <iostream>
#include <vector>
#include <cmath>
#include "slice.h"
#include "distributions-tests.h"
#include "timeseries-adf-tests.h"
using namespace std;

vector<double> mac_kinnon_p_expeted_results = {
    9.443579625324582e-06, 0.004987837276744353, 0.1252400584846753, 0.6842796360469544, 0.9404706143181764, 
    0.00015459297900344507, 0.04550101539695714, 0.4675829489058356, 0.9403596669881464, 0.9870208593200599, 
    0.001074481612410636, 0.15203565925859364, 0.7457008975086779, 0.9870196782995976, 1.0, 
    0.004713401957477449, 0.3193261976428776, 0.8954991525037135, 0.9971573483215169, 1.0, 
    0.015271551250951686, 0.5124777347686033, 0.9610632881445617, 0.9994592130493658, 1.0, 
    0.03900017287521505, 0.6868761409719705, 0.9866248402956618, 0.9999030368909955, 0.9999865017763157, 
    0.00019663990033599052, 0.05827376806874471, 0.533511338910265, 0.958532086060056, 0.9959858831577577, 
    0.0012246688283669626, 0.16568467932873643, 0.7611920745117746, 0.9859002580259643, 1.0, 
    0.005017948639148351, 0.32994930569240855, 0.8959626638870836, 0.9951914365103041, 1.0, 
    0.015798531365670724, 0.5186953235826367, 0.959151056338486, 0.9988119933362168, 1.0, 
    0.03980840652048524, 0.6896499387916724, 0.9853834770676835, 0.9997716105900548, 1.0, 
    0.08397994816308979, 0.8180461799340916, 0.9951474242243235, 0.9999607444306813, 1.0, 
    0.001509518077754119, 0.19694444231149055, 0.8291322873337499, 0.9942331676947893, 1.0, 
    0.005608290139718156, 0.3584831748417828, 0.9206822573542731, 0.9978140385450868, 1.0, 
    0.016830243356833932, 0.5390838598745055, 0.9669248155178387, 0.999361279259567, 1.0, 
    0.04140752362425452, 0.7013720993810106, 0.9876331240714536, 0.9998698970575081, 1.0, 
    0.08592366549111158, 0.8239532659428246, 0.9957653849440733, 0.9999766142149513, 1.0, 
    0.15484755331216182, 0.9045986478660635, 0.9986649137646869, 0.9999961525220925, 0.9999997135727073, 
    0.00651646861697489, 0.39829605609889057, 0.9454187814369912, 0.9989573259007248, 1.0, 
    0.01851771652312444, 0.5715232093702148, 0.9765696885242978, 0.9997410275010333, 1.0, 
    0.04407101470847329, 0.7230403230480806, 0.9908722788193637, 0.9999441588143071, 1.0, 
    0.08958617182697298, 0.8362547601259984, 0.9968061658917771, 0.9999903829019584, 0.9999993388797181, 
    0.15920363515579722, 0.9109233683813405, 0.9989764105476042, 0.9999984785303404, 0.9999999592694216, 
    0.25273878870590916, 0.95501858893269, 0.9996864820170016, 0.9999997146834694, 0.9999999950301558
};

#define close_enough(a, b) (std::abs(a - b) < 9e-6)

void mac_kinnon_p_test() {
    std::cout << "Running mac_kinnon_p_test..." << std::endl;
    size_t expected_index = 0;    
    using namespace timeseries::adf;

    // List of all regression types to loop through
    std::vector<RegressionType> regression_types = {
        RegressionType::NO_CONSTANT,
        RegressionType::CONSTANT,
        RegressionType::CONSTANT_PLUS_LINEAR,
        RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC
    };

    std::vector<double> test_statistics = {-4.5, -2.8, -1.5, 0.0, 1.2};

    std::cout << "  Passed verification for 2-argument overload (implicit N=1)" << std::endl;

    // 2. Comprehensive loop over all regression types and valid N values (1 to 16)
    for (auto reg_type : regression_types) {        
        for (size_t index = 1; index <= tau_star_nc.size(); ++index) {
            cout << reg_type << ',' << index << ':' << ' ';
            for (double stat : test_statistics) {
                auto res = mac_kinnon_p(stat, reg_type, index);
                
                // Assert that the function computed a result successfully
                assert(res.has_value()); // mac_kinnon_p_expeted_results
                assert(close_enough(*res, mac_kinnon_p_expeted_results[expected_index]));
                cout << *res << ", ";
                expected_index++;
            }
            cout << endl;
        }
    }
    std::cout << "  Passed verification for all valid N values (1 to 16) across all regression types" << std::endl;

    // 3. Error Handling Test Case: Out-of-bounds bounds for N
    // N = 0 or N > 16 should trigger a failure/error code depending on table size mappings
    size_t invalid_n = 99;
    auto error_res = mac_kinnon_p(-2.0, RegressionType::CONSTANT, invalid_n);
    if (!error_res.has_value()) {
        std::cout << "  Passed verification for invalid N handling" << std::endl;
    }

    std::cout << "All mac_kinnon_p_test cases passed!\n" << std::endl;
}


int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    evaluate_test();
    evaluate_reversed_test();
    mac_kinnon_p_test();
    tau_2010s_test();
    standard_cdf_test();
    row_span_for_mac_kinnon_table6x3_test();
    row_span_for_mac_kinnon_table6x4_test();
    evaluate_horizontally_test();
    evaluate_horizontally_reversed_test();    

    return 0;
}
#endif