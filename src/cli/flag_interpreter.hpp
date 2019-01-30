/*!
 * @title
 *
 * @author Jackson McNeill
 *
 * Interpret command line options into simple flag / optional argument list.
 */
#pragma once


#include <string>
#include <vector>
#include <map>
#include <optional>
#include <tuple>
#include <exception>
#include <cstring>
#include <memory>
#include <new>

#include "string_manip/string_manip.hpp"

namespace flag_interpreter
{
	struct flag_and_options
	{
		std::string flag;
		std::vector<std::string> option;
	};

	inline bool operator==(const flag_and_options& one, const flag_and_options& two)
	{
		return (one.flag == two.flag) && (one.option == two.option);
	}






	class invalid_shorthand : public std::exception
	{
		const char* suffix = " is not a valid shorthand flag";
		public:
			invalid_shorthand(const char* name)
			{
				msg = new char[strlen(name)+strlen(suffix)];
				strcpy(msg,name);
				strcat(msg,suffix);
			}
			~invalid_shorthand()
			{
				delete[] msg;
			}
			char* msg;
			const char* what() const throw()
			{
				//return (name + std::string(" is not a valid shorthand flag")).c_str();
				return msg;
			}

	};




	using results_t = std::vector<flag_and_options>;
	using shorthands_t = std::map<std::string, std::string>;
	/*!
	 * shorthands format:
	 *
	 * o ; option
	 *
	 * -o will be equal to --option
	 *
	 *
	 *  examples of input to output
	 *  -o test --option=test -yes
	 *
	 *  out: {option,test},{option,test},{yes,}
	 */
	results_t process(int argument_count, char const* const arguments[] , shorthands_t shorthands);
	
	



}
