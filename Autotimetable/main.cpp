// Autotimetable.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iterator>
#include <utility>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <chrono>

#include "intrinsics.hpp"

#include "json.hpp"

#include "autotimetable.hpp"

// returns false if cannot be found
// if found, returns pointer to null if no value, otherwise returns pointer to first char in value
inline bool find_arg(int argc, char* argv[], const char* key, char*& out) {
std::size_t key_len = std::strlen(key);
for (int i = 1; i < argc; ++i) { // note: argv[0] is the program name; it won't be processed
	char* param = argv[i];
	std::size_t param_len = std::strlen(param);
	if (key_len <= param_len && std::equal(key, key + key_len, param)) {
		if (key_len == param_len) {
			out = param + param_len; // '\0'
			return true;
		}
		else if (param[key_len] == '=') {
			out = param + key_len + 1;
			return true;
		}
	}
}
out = nullptr;
return false;
}

inline bool read_required_param(int argc, char* argv[], const char* key, const char* failtext, std::string& out) {
	char* _tmp = nullptr;
	if (find_arg(argc, argv, key, _tmp)) {
		out = std::string(_tmp);
		return true;
	}
	else {
		std::cout << failtext << std::endl;
		return false;
	}
}

inline bool read_optional_param(int argc, char* argv[], const char* key, std::string& out) {
	char* _tmp = nullptr;
	if (find_arg(argc, argv, key, _tmp)) {
		out = std::string(_tmp);
		return true;
	}
	else {
		return false;
	}
}

inline unsigned int parse_day(const std::string& daytext) {
	if (daytext == "Monday")return 0;
	if (daytext == "Tuesday")return 1;
	if (daytext == "Wednesday")return 2;
	if (daytext == "Thursday")return 3;
	if (daytext == "Friday")return 4;
	if (daytext == "Saturday")return 5;
	throw std::invalid_argument("Argument \"" + daytext + "\" not interpretable as day of week.");
}

enum : unsigned int {
	WEEK_ODD = 1, WEEK_EVEN = 2
};

inline unsigned int parse_weektype(const std::string& weektext) {
	if (weektext == "Odd Week")return WEEK_ODD;
	if (weektext == "Even Week")return WEEK_EVEN;
	if (weektext == "Every Week")return WEEK_ODD | WEEK_EVEN;
	if (weektext == "0,1,2,3,4,5,6,7,8,9,10,11,12,13")return WEEK_ODD | WEEK_EVEN;
	throw std::invalid_argument("Argument \"" + weektext + "\" not interpretable as week type.");
}

inline unsigned int parse_weektype_lenient(const std::string& weektext, std::string& error) noexcept {
	if (weektext == "Odd Week")return WEEK_ODD;
	if (weektext == "Even Week")return WEEK_EVEN;
	if (weektext == "Every Week")return WEEK_ODD | WEEK_EVEN;
	if (weektext == "0,1,2,3,4,5,6,7,8,9,10,11,12,13")return WEEK_ODD | WEEK_EVEN;
	error = "Argument \"" + weektext + "\" not interpretable as week type, will be treated as a weekly lesson.";
	return WEEK_ODD | WEEK_EVEN; // if we can't tell, just say its every week for good measure
}

template <typename Callback>
inline autotimetable::timeblock get_timeblock_from_json_choice(const nlohmann::json& json_choice, Callback soft_error_callback) {
	autotimetable::timeblock ret;

	std::string weektext = json_choice["WeekText"].get<std::string>();
	std::string daytext = json_choice["DayText"].get<std::string>();
	std::string starttimetext = json_choice["StartTime"].get<std::string>();
	std::string endtimetext = json_choice["EndTime"].get<std::string>();

	std::string weekerr;
	unsigned int weekmask = parse_weektype_lenient(weektext, weekerr);
	if (!weekerr.empty())soft_error_callback(weekerr);
	unsigned int daynum = parse_day(daytext);
	unsigned int starttime = static_cast<unsigned int>(std::stoul(starttimetext)) / 100;
	unsigned int endtime = (static_cast<unsigned int>(std::stoul(endtimetext)) + 99) / 100; // round times up to nearest hour
	if (daynum >= (autotimetable::TIMEBLOCK_DAY_COUNT >> 1))throw std::invalid_argument("Day of week not valid.");
	if (starttime >= endtime)throw std::invalid_argument("Time range not valid.");

	autotimetable::timeblock_day_t dayres = 0;
	for (unsigned int i = starttime; i < endtime; ++i) {
		dayres |= (1u << i);
	}

	if (weekmask & WEEK_ODD)ret.days[daynum] = dayres;
	if (weekmask & WEEK_EVEN)ret.days[(autotimetable::TIMEBLOCK_DAY_COUNT >> 1) + daynum] = dayres;

	return ret;
}


inline void print_header(std::ostream& out, unsigned begin_index, unsigned end_index, unsigned width) {
	out << std::setw(0) << '|';
	for (unsigned i = begin_index; i < end_index; ++i) {
		out << std::right << std::setw(2) << std::setfill('0') << i << std::setw(width - 2) << std::setfill(' ') << "";
		out << std::setw(0) << '|';
	}
	out << std::endl;
}

inline void print_spacer(std::ostream& out, unsigned begin_index, unsigned end_index, unsigned width) {
	out << std::setw(0) << '+';
	for (unsigned i = begin_index; i < end_index; ++i) {
		out << std::left << std::setw(width) << std::setfill('-') << "";
		out << std::setw(0) << '+';
	}
	out << std::endl;
}

inline void print_modname(std::ostream& out, unsigned begin_index, unsigned end_index, unsigned width, const autotimetable::timetable& timetable, unsigned dayindex) {
	out << std::setw(0) << '|';
	for (unsigned i = begin_index; i < end_index; ) {
		auto it = std::find_if(timetable.items.cbegin(), timetable.items.cend(), [i, dayindex](const std::tuple<typename std::vector<autotimetable::mod>::const_iterator, typename std::vector<autotimetable::mod_item>::const_iterator, typename std::vector<autotimetable::mod_item_choice>::const_iterator>& choice) {
			return (std::get<2>(choice)->timeblock.days[dayindex] & (1u << i));
		});
		if (it != timetable.items.cend()) {
			unsigned modiend = i + 1;
			while (modiend < end_index && std::get<2>(*it)->timeblock.days[dayindex] & (1u << modiend))++modiend;
			unsigned eff_width = (width + 1) * (modiend - i) - 1;
			out << std::left << std::setw(eff_width) << std::setfill(' ') << std::get<0>(*it)->code.substr(0, eff_width);
			i = modiend;
		}
		else {
			out << std::left << std::setw(width) << std::setfill(' ') << "";
			++i;
		}
		out << std::setw(0) << '|';
		/*if (it != timetable.items.cend() && (i == begin_index || !(std::get<2>(*it)->timeblock.days[dayindex] & (1u << (i - 1))))) {
			out << std::left << std::setw(width) << std::setfill(' ') << std::get<0>(*it)->code.substr(0, width);
		}
		else {
			out << std::left << std::setw(width) << std::setfill(' ') << "";
		}
		if (it != timetable.items.cend() && i < end_index - 1 && (std::get<2>(*it)->timeblock.days[dayindex] & (1u << i)) && (std::get<2>(*it)->timeblock.days[dayindex] & (1u << (i + 1)))) {
			out << std::setw(0) << ' ';
		}
		else {
			out << std::setw(0) << '|';
		}*/
	}
	out << std::endl;
}

inline void print_modkind(std::ostream& out, unsigned begin_index, unsigned end_index, unsigned width, const autotimetable::timetable& timetable, unsigned dayindex) {
	out << std::setw(0) << '|';
	for (unsigned i = begin_index; i < end_index; ) {
		auto it = std::find_if(timetable.items.cbegin(), timetable.items.cend(), [i, dayindex](const std::tuple<typename std::vector<autotimetable::mod>::const_iterator, typename std::vector<autotimetable::mod_item>::const_iterator, typename std::vector<autotimetable::mod_item_choice>::const_iterator>& choice) {
			return (std::get<2>(choice)->timeblock.days[dayindex] & (1u << i));
		});
		if (it != timetable.items.cend()) {
			unsigned modiend = i + 1;
			while (modiend < end_index && std::get<2>(*it)->timeblock.days[dayindex] & (1u << modiend))++modiend;
			unsigned eff_width = (width + 1) * (modiend - i) - 1;
			out << std::left << std::setw(eff_width) << std::setfill(' ') << std::get<1>(*it)->kind.substr(0, eff_width);
			i = modiend;
		}
		else {
			out << std::left << std::setw(width) << std::setfill(' ') << "";
			++i;
		}
		out << std::setw(0) << '|';
	}
	out << std::endl;
}

inline void print_modchoice(std::ostream& out, unsigned begin_index, unsigned end_index, unsigned width, const autotimetable::timetable& timetable, unsigned dayindex) {
	out << std::setw(0) << '|';
	for (unsigned i = begin_index; i < end_index; ) {
		auto it = std::find_if(timetable.items.cbegin(), timetable.items.cend(), [i, dayindex](const std::tuple<typename std::vector<autotimetable::mod>::const_iterator, typename std::vector<autotimetable::mod_item>::const_iterator, typename std::vector<autotimetable::mod_item_choice>::const_iterator>& choice) {
			return (std::get<2>(choice)->timeblock.days[dayindex] & (1u << i));
		});
		if (it != timetable.items.cend()) {
			unsigned modiend = i + 1;
			while (modiend < end_index && std::get<2>(*it)->timeblock.days[dayindex] & (1u << modiend))++modiend;
			unsigned eff_width = (width + 1) * (modiend - i) - 1;
			out << std::left << std::setw(eff_width) << std::setfill(' ') << std::get<2>(*it)->name.substr(0, eff_width);
			i = modiend;
		}
		else {
			out << std::left << std::setw(width) << std::setfill(' ') << "";
			++i;
		}
		out << std::setw(0) << '|';
	}
	out << std::endl;
}


inline std::tuple<std::string, std::string, std::string> parse_fixed_mod(const std::string& fixed_mods_str, std::size_t begin, std::size_t end) {
	std::size_t c1 = fixed_mods_str.find(':', begin);
	if (c1 == std::string::npos) {
		throw std::invalid_argument("Cannot parse fixed mod selection: \"" + fixed_mods_str.substr(begin, end - begin) + "\".");
	}
	std::size_t c2 = fixed_mods_str.find(':', c1 + 1);
	if (c2 == std::string::npos) {
		throw std::invalid_argument("Cannot parse fixed mod selection: \"" + fixed_mods_str.substr(begin, end - begin) + "\".");
	}
	std::size_t c3 = fixed_mods_str.find(':', c2 + 1);
	if (c3 < end) {
		throw std::invalid_argument("Cannot parse fixed mod selection: \"" + fixed_mods_str.substr(begin, end - begin) + "\".");
	}
	return std::tuple<std::string, std::string, std::string>(fixed_mods_str.substr(begin, c1 - begin), fixed_mods_str.substr(c1 + 1, c2 - (c1 + 1)), fixed_mods_str.substr(c2 + 1, end - (c2 + 1)));
}


int main(int argc, char *argv[]) {
	std::string modulefilepath;
	if (!read_required_param(argc, argv, "--modulefile", "Fatal error: module file not specified.  Use \"--modulefile=<filename>\".", modulefilepath))return 0;


	std::string required_mods_str;
	if (!read_required_param(argc, argv, "--required", "Fatal error: required modules not specified.  Use \"--required=<module code 1>,<module code 2>,...\", e.g. \"--required=CS1010,MA1101R,CS1231,BN1101,GET1002\" (without spaces).", required_mods_str))return 0;

	std::string fixed_mods_str;
	read_optional_param(argc, argv, "--fixed", fixed_mods_str);
	for (char& ch : fixed_mods_str) {
		if (ch == '-' || ch == '_')ch = ' ';
	}


	bool quiet = false;
	{
		std::string quiet_str;
		if (read_optional_param(argc, argv, "--quiet", quiet_str)) {
			if (quiet_str.empty() || quiet_str == "true" || quiet_str == "t" || quiet_str == "1") {
				quiet = true;
			}
			else if (quiet_str == "false" || quiet_str == "f" || quiet_str == "0") {
				// already default false
			}
			else {
				std::cout << "Warning: Cannot interpret value for --quiet, ignoring it." << std::endl;
			}
		}
	}

	autotimetable::score_config scorer = autotimetable::default_config();
	{
		std::string override_empty_slot_penalty;
		if (read_optional_param(argc, argv, "--empty-slot", override_empty_slot_penalty)) {
			try {
				scorer.empty_slot_penalty = static_cast<autotimetable::score_t>(std::stoul(override_empty_slot_penalty));
			}
			catch (...) {
				std::cout << "Warning: Cannot interpret value for --empty-slot, ignoring it." << std::endl;
			}
		}
	}
	{
		std::string override_travel_penalty;
		if (read_optional_param(argc, argv, "--travel", override_travel_penalty)) {
			try {
				scorer.travel_penalty = static_cast<autotimetable::score_t>(std::stoul(override_travel_penalty));
			}
			catch (...) {
				std::cout << "Warning: Cannot interpret value for --travel, ignoring it." << std::endl;
			}
		}
	}
	{
		std::string override_no_lunch_penalty;
		if (read_optional_param(argc, argv, "--no-lunch", override_no_lunch_penalty)) {
			if (override_no_lunch_penalty.empty()) {
				scorer.no_lunch_penalty = 0;
			}
			else {
				try {
					scorer.no_lunch_penalty = static_cast<autotimetable::score_t>(std::stoul(override_no_lunch_penalty));
				}
				catch (...) {
					std::cout << "Warning: Cannot interpret value for --no-lunch, ignoring it." << std::endl;
				}
			}
		}
	}
	{
		std::string override_lunch_start, override_lunch_end;
		bool has_lunch_start = read_optional_param(argc, argv, "--lunch-start", override_lunch_start);
		bool has_lunch_end = read_optional_param(argc, argv, "--lunch-end", override_lunch_end);
		if (has_lunch_start && !has_lunch_end) {
			std::cout << "Warning: --lunch-start specified without --lunch-end, ignoring it." << std::endl;
		}
		else if (!has_lunch_start && has_lunch_end) {
			std::cout << "Warning: --lunch-end specified without --lunch-start, ignoring it." << std::endl;
		}
		else if (has_lunch_start && has_lunch_end) {
			try {
				autotimetable::score_t tmp_lunch_start = static_cast<autotimetable::score_t>(std::stoul(override_lunch_start));
				autotimetable::score_t tmp_lunch_end = static_cast<autotimetable::score_t>(std::stoul(override_lunch_end));
				if (tmp_lunch_start % 100 || tmp_lunch_end % 100) {
					std::cout << "Warning: --lunch-start and --lunch-end must end with '00', ignoring them." << std::endl;
				}
				else if (tmp_lunch_start > 2300)std::cout << "Warning: --lunch-start larger than 2300, ignoring it." << std::endl;
				else if (tmp_lunch_end > 2400)std::cout << "Warning: --lunch-end larger than 2400, ignoring it." << std::endl;
				else if (tmp_lunch_start >= tmp_lunch_end)std::cout << "Warning: --lunch-end must be later than --lunch-start, ignoring it." << std::endl;
				else {
					tmp_lunch_start /= 100;
					tmp_lunch_end /= 100;
					scorer.lunch_time = (1u << tmp_lunch_end) - (1u << tmp_lunch_start);
				}
			}
			catch (...) {
				std::cout << "Warning: Cannot interpret value for --lunch-start or --lunch-end, ignoring them." << std::endl;
			}
		}
	}




	std::vector<std::string> required_mods;
	if (!required_mods_str.empty()) {
		std::size_t curr = 0;
		while (true) {
			std::size_t next = required_mods_str.find(',', curr);
			if (next == std::string::npos) {
				required_mods.emplace_back(required_mods_str.substr(curr));
				break;
			}
			else {
				required_mods.emplace_back(required_mods_str.substr(curr, next - curr));
			}
			curr = next + 1;
		}
	}


	std::vector<std::tuple<std::string, std::string, std::string>> fixed_mods;
	if (!fixed_mods_str.empty()) {
		std::size_t curr = 0;
		while (true) {
			std::size_t next = fixed_mods_str.find(',', curr);
			try {
				if (next == std::string::npos) {
					fixed_mods.emplace_back(parse_fixed_mod(fixed_mods_str, curr, fixed_mods_str.size()));
					break;
				}
				else {
					fixed_mods.emplace_back(parse_fixed_mod(fixed_mods_str, curr, next));
				}
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << "  Selection will be ignored.  Use \"--fixed=<selection 1>,<selection 2>,...\", where <selection #> = \"<module code>:<lesson kind>:<selection name>\", e.g. \"CS1010:Tutorial:T01\"." << std::endl;
				if (next == std::string::npos) {
					break;
				}
			}
			curr = next + 1;
		}
	}





	std::vector<autotimetable::mod> all_mods;

	std::map<std::string, std::vector<autotimetable::mod>::const_iterator> mod_index;

	std::cout << "Loading modules..." << std::endl;
	{
		nlohmann::json json_data;
		try{
			std::ifstream in(modulefilepath);
			in >> json_data;
		}
		catch (std::ios_base::failure& e) {
			std::cout << "Fatal error: " << e.what() << std::endl;
			return 0;
		}
		std::for_each(std::make_move_iterator(json_data.begin()), std::make_move_iterator(json_data.end()), [&all_mods, quiet](nlohmann::json&& json_module) {
			autotimetable::mod mod;
			try {
				mod.code = json_module["ModuleCode"].get<std::string>();
				std::map<std::string, std::map<std::string, autotimetable::mod_item_choice>> choices; // {kind, collection of {choicename, choice}}
				nlohmann::json& json_timeoptions = json_module["Timetable"];
				std::for_each(std::make_move_iterator(json_timeoptions.begin()), std::make_move_iterator(json_timeoptions.end()), [&choices, quiet, &mod_code = mod.code](nlohmann::json&& json_choice) {
					std::string kind = json_choice["LessonType"].get<std::string>();
					std::map<std::string, autotimetable::mod_item_choice>& mod_item_choices = choices.emplace(std::move(kind), std::map<std::string, autotimetable::mod_item_choice>{}).first->second;
					std::string choice_name = json_choice["ClassNo"].get<std::string>();
					autotimetable::mod_item_choice& choice = mod_item_choices.emplace(choice_name, autotimetable::mod_item_choice{ choice_name }).first->second;
					choice.timeblock.add(get_timeblock_from_json_choice(json_choice, [quiet, &mod_code](const std::string& err) {
						if (!quiet)std::cout << "Soft warning for module " << mod_code << ": " << err << std::endl;
					}));
				});
				mod.items.reserve(choices.size());
				std::transform(std::make_move_iterator(choices.begin()), std::make_move_iterator(choices.end()), std::back_inserter(mod.items), [](std::pair<std::string, std::map<std::string, autotimetable::mod_item_choice>>&& choice_kind) {
					std::vector<autotimetable::mod_item_choice> ret_choices;
					ret_choices.reserve(choice_kind.second.size());
					std::transform(std::make_move_iterator(choice_kind.second.begin()), std::make_move_iterator(choice_kind.second.end()), std::back_inserter(ret_choices), [](std::pair<std::string, autotimetable::mod_item_choice>&& choice) {
						return std::move(choice.second);
					});
					return autotimetable::mod_item{ std::move(choice_kind.first), std::move(ret_choices) };
				});
			}
			catch (const std::invalid_argument& err) {
				if (!quiet)std::cout << "Skipping module " << mod.code << " as we cannot interpret it: " << err.what() << std::endl;
				return;
			}
			all_mods.emplace_back(std::move(mod));
		});
		all_mods.shrink_to_fit();
	}
	std::cout << "Done loading modules." << std::endl;

	std::cout << "Building index..." << std::endl;
	for (auto it = all_mods.cbegin(); it != all_mods.cend(); ++it) {
		if (!mod_index.emplace(it->code, it).second) {
			std::cout << "Duplicate module " << it->code << ", module will be skipped" << std::endl;
		}
	}
	std::cout << "Done building index." << std::endl;

	std::vector<autotimetable::mod> selected_mods;

	std::cout << "Preparing parameters for autotimetable..." << std::endl;
	for (const std::string& mod_title : required_mods) {
		auto it = mod_index.find(mod_title);
		if (it == mod_index.end()) {
			std::cout << "Cannot find module " << mod_title << " as requested, module will be ignored" << std::endl;
		}
		else {
			selected_mods.emplace_back(*(it->second)); // *copies* the autotimetable::mod
		}
	}
	for (const std::tuple<std::string, std::string, std::string>& fix : fixed_mods) {
		const std::string& mod_code = std::get<0>(fix);
		auto it1 = std::find_if(selected_mods.begin(), selected_mods.end(), [&mod_code](const autotimetable::mod& mod){
			return mod.code == mod_code;
		});
		if (it1 == selected_mods.end()) {
			std::cout << "Cannot find module code \"" << mod_code << "\", fixed module constraint will be ignored" << std::endl;
		}
		else {
			auto& items = it1->items;
			const std::string& mod_kind = std::get<1>(fix);
			auto it2 = std::find_if(items.begin(), items.end(), [&mod_kind](const autotimetable::mod_item& item) {
				return item.kind == mod_kind;
			});
			if (it2 == items.end()) {
				std::cout << "Cannot find module kind \"" << mod_kind << "\", fixed module constraint will be ignored" << std::endl;
			}
			else {
				auto& choices = it2->choices;
				const std::string& mod_choice = std::get<2>(fix);
				auto it3 = std::find_if(choices.begin(), choices.end(), [&mod_choice](const autotimetable::mod_item_choice& choice) {
					return choice.name == mod_choice;
				});
				if (it3 == choices.end()) {
					std::cout << "Cannot find module choice \"" << mod_choice << "\", fixed module constraint will be ignored" << std::endl;
				}
				else {
					// delete all the choices except this one:
					autotimetable::mod_item_choice choice = std::move(*it3);
					choices.clear();
					choices.emplace_back(std::move(choice));
				}
			}
		}
	}
	std::cout << "Done preparing." << std::endl;

	std::cout << "Running autotimetable..." << std::endl;
	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
	autotimetable::timetable find_result = autotimetable::find_best(selected_mods, scorer);
	auto milliseconds_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
	std::cout << "Done running autotimetable..." << std::endl;

	std::cout << std::endl;

	// find the first and last hours to print
	unsigned begin_index = std::accumulate(find_result.items.cbegin(), find_result.items.cend(), 24u, [](unsigned prev, const std::tuple<typename std::vector<autotimetable::mod>::const_iterator, typename std::vector<autotimetable::mod_item>::const_iterator, typename std::vector<autotimetable::mod_item_choice>::const_iterator>& curr) {
		return std::min(prev, std::accumulate(std::get<2>(curr)->timeblock.days, std::get<2>(curr)->timeblock.days + autotimetable::TIMEBLOCK_DAY_COUNT, 24u, [](unsigned prev, const autotimetable::timeblock_day_t& curr) {
			if (curr == 0)return prev;
			return std::min(prev, intrinsics::find_smallest_set(curr));
		}));
	});
	unsigned end_index = std::accumulate(find_result.items.cbegin(), find_result.items.cend(), 0u, [](unsigned prev, const std::tuple<typename std::vector<autotimetable::mod>::const_iterator, typename std::vector<autotimetable::mod_item>::const_iterator, typename std::vector<autotimetable::mod_item_choice>::const_iterator>& curr) {
		return std::max(prev, std::accumulate(std::get<2>(curr)->timeblock.days, std::get<2>(curr)->timeblock.days + autotimetable::TIMEBLOCK_DAY_COUNT, 0u, [](unsigned prev, const autotimetable::timeblock_day_t& curr) {
			if (curr == 0)return prev;
			return std::max(prev, intrinsics::find_largest_set(curr));
		}));
	}) + 1;

	if (begin_index >= end_index) {
		std::cout << "No suitable timetable found." << std::endl;
	}
	else {

		// print the result nicely
		std::cout << "Here is the result:" << std::endl;
		std::cout << std::endl;
		std::cout << "=== Odd Week ===" << std::endl;
		print_spacer(std::cout, begin_index, end_index, 8);
		print_header(std::cout, begin_index, end_index, 8);
		print_spacer(std::cout, begin_index, end_index, 8);
		for (unsigned i = 0; i < 6; ++i) {
			if (i % 6 != 5 || find_result.timeblock.days[i] != 0) {
				print_modname(std::cout, begin_index, end_index, 8, find_result, i);
				print_modkind(std::cout, begin_index, end_index, 8, find_result, i);
				print_modchoice(std::cout, begin_index, end_index, 8, find_result, i);
				print_spacer(std::cout, begin_index, end_index, 8);
			}
		}
		std::cout << std::endl;
		std::cout << "=== Even Week ===" << std::endl;
		print_spacer(std::cout, begin_index, end_index, 8);
		print_header(std::cout, begin_index, end_index, 8);
		print_spacer(std::cout, begin_index, end_index, 8);
		for (unsigned i = 6; i < 12; ++i) {
			if (i % 6 != 5 || find_result.timeblock.days[i] != 0) {
				print_modname(std::cout, begin_index, end_index, 8, find_result, i);
				print_modkind(std::cout, begin_index, end_index, 8, find_result, i);
				print_modchoice(std::cout, begin_index, end_index, 8, find_result, i);
				print_spacer(std::cout, begin_index, end_index, 8);
			}
		}
		
	}
	std::cout << std::endl;
	std::cout << "Autotimetable executed in " << milliseconds_elapsed << " ms." << std::endl;

	return 0;
}

