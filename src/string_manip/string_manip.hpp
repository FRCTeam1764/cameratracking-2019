/*!
 * @title
 *
 * @author Jackson McNeill
 *
 * Basic string manipulation functions
 */
#pragma once



#include <cstring>
#include <cstdlib>
#include <optional>
#include <tuple>
#include <vector>
#include <array>
#include <algorithm>

namespace string_manip
{
	/*!
	 * If string1 and string2 where std::strings, then this function is string1+string2
	 *
	 * Allocated with new
	 */
	inline char* value_return_strcat(const char* string1, const char* string2)
	{
		char* result = new char[strlen(string1)+strlen(string2)];
		strcpy(result,string1);
		strcat(result,string2);
		return result;
	}
	inline std::optional<std::tuple<std::string,std::string>> get_before_and_after_sequence(std::string input,std::string sequence)
	{
		auto equals_pos = input.find(sequence);
		if (equals_pos != std::string::npos)
		{
			return {std::make_tuple(input.substr(0,equals_pos),input.substr(equals_pos+1))};
		}
		return {};
	}
	inline std::vector<std::string> seperate_comma_seperated_string(std::string input)
	{
		std::vector<std::string> output;

		size_t comma_pos = 0;
		size_t last_comma_pos= static_cast<size_t>(-1); //-1 will not actually be passed so it can be any value that ++ will result in a 0 integer overflow or not.

		bool cont = true;
		while(cont)
		{
			comma_pos = input.find(",",last_comma_pos+1);
			if (comma_pos == std::string::npos)
			{
				cont = false;
				comma_pos = input.size();
			}
			output.push_back(input.substr(last_comma_pos+1,comma_pos-(last_comma_pos+1)));
			last_comma_pos = comma_pos;
		}
		return output;
	}
	inline std::string concat_vector_of_string(std::vector<std::string> input, std::string seperator)
	{
		std::string output = "";
		for (auto this_str : input)
		{
			output += this_str+seperator;
		}
		return output;
	}

	/*!
	 * Constexpr compares 2 NULL-terminated strings or char variants of STL strings
	 */
	constexpr bool constexpr_string_comp(std::string_view a, std::string_view b)
	{
		for (std::string_view::size_type i=0;(a[i] != '\0' && b[i] != '\0');i++)
		{
			if (a[i] != b[i])
			{
				return false;
			}
		}
		return true;
	}
	template<size_t size>
	constexpr bool constexpr_string_comp_all_true(std::array<std::string_view,size> b_array)
	{
		for (std::string_view& b : b_array)
		{
			if (!constexpr_string_comp(b_array[0],b))
			{
				return false;
			}
		}
		return true;
	}
	template<size_t size>
	constexpr bool constexpr_string_comp_one_true(std::string_view a, std::array<std::string_view,size> b_array)
	{
		for (std::string_view& b : b_array)
		{
			if (constexpr_string_comp(a,b))
			{
				return true;
			}
		}
		return false;
	}
	static_assert(constexpr_string_comp(std::string_view("hello"),std::string_view("hello")));
	static_assert(constexpr_string_comp_one_true(std::string_view("hello"),std::array<std::string_view,3>{"hi","bye","hello"}));
	inline std::string touppper(std::string in)
	{
		for (char& c : in)
		{
			c = std::toupper(c);
		}
		return in;
	}
	inline std::string toulower(std::string in)
	{
		for (char& c : in)
		{
			c = std::tolower(c);
		}
		return in;
	}
	inline constexpr std::string_view substr_by_position(std::string_view in, size_t begin, size_t end)
	{
		return std::string_view(&in[begin],end-begin+1);
	}
	/*
	template<size_t begin, size_t end>
	inline constexpr const char* substr_by_position(std::string_view in)
	{
		char out[(end+2)-begin] = {0,}; 
		for (size_t i=0;i<=end;i++)
		{
			out[i] = in[begin+i];
		}
		out[begin+end-1] = '\0';
		return const_cast<const char*>(out);
	}
	*/
	static_assert(substr_by_position("test",1,2)[0] == 'e');
	static_assert(substr_by_position("test",1,2)[1] == 's');



}
