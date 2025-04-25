#include <cerrno>
#include <climits>
#include <cassert>

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include "utils.hpp"

#define OPT_USAGE -3


std::string indexpair_to_string(size_t left, size_t right)
{
	std::stringstream ss;
	ss.width(5);
	ss << left << '-' << std::left << right;
	return ss.str();
}


/** are floats close, i.e. approximately equal? due to floating point
 * representation we have to care about a couple of types of error, relative
 * and absolute.
 */
bool
are_vals_close(const double a, const double b, const double drel, const double dabs)
{
	assert(drel >= 0);
	assert(dabs >= 0);
	assert(drel > 0 || dabs > 0);

	const auto mag = std::max(std::abs(a), std::abs(b));

	return std::abs(b - a) < (drel * mag + dabs);
}

#ifdef INCLUDE_TESTS
TEST_CASE("are_vals_close") {
	SECTION("identical values") {
		CHECK(are_vals_close(0, 0));
		CHECK(are_vals_close(1, 1));
	}
	SECTION("close values") {
		CHECK(are_vals_close(0, 1e-15));
		CHECK(are_vals_close(1, 1+1e-15));
	}
	SECTION("far values") {
		CHECK_FALSE(are_vals_close(0, 1));
		CHECK_FALSE(are_vals_close(1, 0));
		CHECK_FALSE(are_vals_close(0, 1e-10));
	}
}
#endif

bool int_of_string(const char *s, int &i, int base)
{
	char *end;
	long  l;
	errno = 0;
	l = strtol(s, &end, base);
	if ((errno == ERANGE && l == LONG_MAX) || l > std::numeric_limits<int>::max()) {
		return false;
	}
	if ((errno == ERANGE && l == LONG_MIN) || l < std::numeric_limits<int>::min()) {
		return false;
	}
	if (*s == '\0' || *end != '\0') {
		return false;
	}
	i = (int)l;
	return true;
}

bool size_t_of_string(const char *s, size_t &i, int base)
{
	char *end;
	long  l;
	errno = 0;
	l = strtol(s, &end, base);
	if (errno == ERANGE && l == LONG_MAX) {
		return false;
	}
	if ((errno == ERANGE && l == LONG_MIN) || l < 0) {
		return false;
	}
	if (*s == '\0' || *end != '\0') {
		return false;
	}
	i = (size_t)l;
	return true;
}

#ifdef INCLUDE_TESTS
TEST_CASE("int_of_string") {
	SECTION("success") {
		int val = -1;
		CHECK(int_of_string("0", val));
		CHECK(val == 0);
		CHECK(int_of_string("1", val));
		CHECK(val == 1);
		CHECK(int_of_string("-1", val));
		CHECK(val == -1);
		CHECK(int_of_string("0x10", val));
		CHECK(val == 16);
		CHECK(int_of_string("ff", val, 16));
		CHECK(val == 255);
	}
	SECTION("failure") {
		int val = -1;
		CHECK_FALSE(int_of_string("", val));
		CHECK(val == -1);
		CHECK_FALSE(int_of_string("zzz", val));
		CHECK(val == -1);
	}
}
TEST_CASE("size_t_of_string") {
	SECTION("success") {
		size_t val = 1;
		CHECK(size_t_of_string("0", val));
		CHECK(val == 0);
		CHECK(size_t_of_string("1", val));
		CHECK(val == 1);
		CHECK(size_t_of_string("0x10", val));
		CHECK(val == 16);
		CHECK(size_t_of_string("ff", val, 16));
		CHECK(val == 255);
	}
	SECTION("failure") {
		size_t val = 7;
		CHECK_FALSE(size_t_of_string("", val));
		CHECK(val == 7);
		CHECK_FALSE(size_t_of_string("-1", val));
		CHECK(val == 7);
		CHECK_FALSE(size_t_of_string("18446744073709551616", val));
		CHECK(val == 7);
		CHECK_FALSE(size_t_of_string("zzz", val));
		CHECK(val == 7);
	}
}
#endif

enum class CSVState {
	UnquotedField,
	QuotedField,
	QuotedQuote
};

// copied from https://stackoverflow.com/a/30338543/1358308
std::vector<std::string>
parse_csv_row(const std::string &row)
{
	CSVState state = CSVState::UnquotedField;
	std::vector<std::string> fields {""};
	size_t i = 0; // index of the current field
	for (char c : row) {
		switch (state) {
		case CSVState::UnquotedField:
			switch (c) {
			case ',': // end of field
				fields.push_back(""); i++;
				break;
			case '"':
				state = CSVState::QuotedField;
				break;
			default:  fields[i].push_back(c);
				break; }
			break;
		case CSVState::QuotedField:
			switch (c) {
			case '"':
				state = CSVState::QuotedQuote;
				break;
			default:  fields[i].push_back(c);
				break; }
			break;
		case CSVState::QuotedQuote:
			switch (c) {
			case ',': // , after closing quote
				fields.push_back(""); i++;
				state = CSVState::UnquotedField;
				break;
			case '"': // "" -> "
				fields[i].push_back('"');
				state = CSVState::QuotedField;
				break;
			default:  // end of quote
				state = CSVState::UnquotedField;
				break; }
			break;
		}
	}
	return fields;
}

#ifdef INCLUDE_TESTS
template<typename T> bool
vectors_eq(const std::vector<T> &a, const std::vector<T> &b)
{
	return a.size() == b.size() &&
		std::equal(a.begin(), a.end(), b.begin());
}

TEST_CASE("parse_csv_row") {
	SECTION("validate vectors_eq") {
		CHECK(vectors_eq<int>({}, {}));
		CHECK(vectors_eq<int>({0}, {0}));
		CHECK_FALSE(vectors_eq<int>({0}, {}));
		CHECK_FALSE(vectors_eq<int>({}, {1}));
		CHECK_FALSE(vectors_eq<int>({0}, {1}));
	}
	SECTION("simple cases") {
		CHECK(vectors_eq(parse_csv_row(""), {""}));
		CHECK(vectors_eq(parse_csv_row("a"), {"a"}));
		CHECK(vectors_eq(parse_csv_row(","), {"",""}));
		CHECK(vectors_eq(parse_csv_row(",a"), {"","a"}));
		CHECK(vectors_eq(parse_csv_row("a, b"), {"a"," b"}));
		CHECK(vectors_eq(parse_csv_row("a ,b"), {"a ","b"}));
	}
	SECTION("double-quote escapes") {
		CHECK(vectors_eq(parse_csv_row("\"\""), {""}));
		CHECK(vectors_eq(parse_csv_row("\",\""), {","}));
		CHECK(vectors_eq(parse_csv_row("\"\"\"\""), {"\""}));
	}
}
#endif


input_status
parse_next_row(std::istream &is, std::vector<std::string> &row)
{
	if (is.eof()) {
		return input_status::end_of_file;
	}
	std::string line;
	if (std::getline(is, line).fail()) {
		return input_status::end_of_file;
	}
	if (is.bad()) {
		return input_status::error;
	}

	row = parse_csv_row(line);

	return input_status::success;
}
