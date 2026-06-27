#ifdef __TEST_MAIN__
#ifndef TRADING_ENGINE_PORTFOLIO_TESTS_H
#define TRADING_ENGINE_PORTFOLIO_TESTS_H

void test_create_drawdowns(); // Covers DrawDowns::Create
void test_portfolio_create(const char * current_program_path);
void test_portfolio_update_timeindex(const char * current_program_path);
void test_update_holdings_from_fill(const char * current_program_path);
void test_update_positions_from_fill(const char * current_program_path);
void test_update_fill(const char * current_program_path);

#endif
#endif