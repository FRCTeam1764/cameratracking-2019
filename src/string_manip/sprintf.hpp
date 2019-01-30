/*!
 * @file
 *
 * @author Jackson McNeill
 * 
 * Gives a constexpr sprintf function
 *
 * Allows specification of arbitrary %[word] types.
 */
#pragma once
#include <string>
#include "string_manip.hpp"
#include <type_traits>
#include <array>
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace string_manip
{
	
	constexpr size_t count_formats_in_formatstr(std::string_view formatstr)
	{
		size_t num_formats = 0;
		for(size_t i=0;formatstr[i]!='\0';i++)
		{
			if (formatstr[i] == '%')
			{
				if (formatstr[i+1] == '%')
				{
					i++;
					continue;
				}
				num_formats++;
				i++;
				//progress to the terminating %
				for(;(formatstr[i]!='%' && formatstr[i] != '\0');i++){}
			}
		}
		return num_formats;
	}
	struct formatstr_position
	{
		size_t begin;
		size_t end;
	};
	struct formatstr_info
	{
		formatstr_position format;
	};
	template<const char* formatstr>
	constexpr auto format_str_parse() 
	{
		constexpr size_t number_of_specifiers = count_formats_in_formatstr(formatstr);
		std::array<formatstr_info,number_of_specifiers> output = {};
		unsigned int spot = 0;
		for(size_t i=0;formatstr[i]!='\0';i++)
		{
			if (formatstr[i] == '%')
			{
				if (formatstr[i+1] == '%')
				{
					i++;
					continue;
				}
				size_t begin = i;
				i++;
				//progress to the terminating %
				for(;(formatstr[i]!='%' && formatstr[i] != '\0');i++){}

				output[spot] = formatstr_info{begin,i};
				spot++;
			}
		}
		return output;
	}

	constexpr char test[] = "%d%34%f%";
	static_assert(format_str_parse<test>()[0].format.begin == 0);
	static_assert(format_str_parse<test>()[0].format.end == 2);
	static_assert(format_str_parse<test>()[1].format.begin == 5);
	static_assert(format_str_parse<test>()[1].format.end == 7);

	inline std::string double_to_scientific_string(double in, int precision,bool uppercase)
	{
		std::ostringstream out;
		out << std::scientific << (uppercase? std::uppercase : std::nouppercase) << std::setprecision(precision) << in;
		return out.str();
	}
	template<typename T>
	inline std::string to_hex(T in, int precision, bool uppercase)
	{
		std::ostringstream out;
		out << std::hex << (uppercase? std::uppercase : std::nouppercase) << in;
		return out.str();
	}

	constexpr std::array<std::string_view,14> basic_formats_specifiers = 
	{
		"d", //signed int
		"i",
		"u", //unsigned int
		"f",//double (fixed form)
		"F",
		"e", //double(exponent form)
		"E",
		"g", //decimal/exponent double form
		"G",
		"x",//hexidecimal lower
		"X",//upper
		"s",//string (also supports C++ strings)
		"a",//double in hexidecimal
		"A",
	};
	template<const char* specifier, unsigned int begin, unsigned int length, typename T>
	inline std::enable_if_t<constexpr_string_comp_one_true(std::string_view(specifier+begin,length),basic_formats_specifiers),std::string>
	sprintf_format_handler(T data)
	{
		if constexpr(specifier[begin] == 'd' || specifier[begin] == 'i')
		{
			return std::to_string(static_cast<long long int>(data));
		}
		if constexpr(specifier[begin] == 'u')
		{
			return std::to_string(static_cast<unsigned long long int>(data));
		}
		if constexpr(specifier[begin] == 'f')
		{
			return lowercase(std::to_string(data));
		}
		if constexpr(specifier[begin] == 'F')
		{
			return uppercase(std::to_string(data));
		}
		if constexpr(specifier[begin] == 'e')
		{
			return double_to_scientific_string(data,99,false);
		}
		if constexpr(specifier[begin] == 'E')
		{
			return double_to_scientific_string(data,99,true);
		}
		//TODO: this could be made ~2x more effecient
		if constexpr(specifier[begin] == 'g')
		{
			auto decimal = sprintf_format_handler<'f',1>(data);
			auto exp = sprintf_format_handler<'e',1>(data);
			return decimal.length() > exp.length()? exp : decimal;
		}
		if constexpr(specifier[begin] == 'G')
		{
			auto decimal = sprintf_format_handler<'F',1>(data);
			auto exp = sprintf_format_handler<'E',1>(data);
			return decimal.length() > exp.length()? exp : decimal;
		}
		if constexpr(specifier[begin] == 'a' || specifier[begin] == 'x')
		{
			return to_hex(data,99,false);
		}
		if constexpr(specifier[begin] == 'A' || specifier[begin] == 'X')
		{
			return to_hex(data,99,true);
		}
		if constexpr(specifier[begin] == 's')
		{
			return std::string(data);
		}
	}

	template <size_t loop, typename T, typename Y>
	constexpr void static_for(T array,Y lambda)
	{
		auto var = array[loop];
		constexpr auto begin = var.begin;
		constexpr auto end = var.end;
		lambda(var,loop,begin,end);
		if constexpr (loop+1 < array.size())
		{
			static_for<loop+1>(array,lambda);	
		}
	}


	/*!
	 * A C-like sprintf function WITH A KEY DIFFERENCE: Formats are terminated with %.
	 *
	 * Example: you have %d% cards,5  -> you have 5 cards.
	 */
	/*
	template<const char* formatstr, typename ...T>
	inline constexpr auto sprintf(T ... args)
	{
		constexpr auto specifiers = std::array<formatstr_info,1>({formatstr_info{1,2}});//format_str_parse<formatstr>();
		//size_t additional_length = 0;
		//std::string out = "";
		static_for<0>(specifiers,[](formatstr_info info,size_t time, size_t begin, size_t end)
		{
			//out += substr_by_position(formatstr,additional_length,info.begin-1);
			constexpr auto format = substr_by_position(formatstr,1,1);
			//std::string result = sprintf_format_handler<format,format.length()>(std::get<time>(args...));
			//out += result;
			//additional_length = info.end+1;
		});
		return "";
	}
	*/

	template<size_t number, const char* formatstr, typename ...T>
	inline auto sprintf(T ... args)
	{
		constexpr size_t number_of_specifiers = count_formats_in_formatstr(formatstr);
		static_assert(number_of_specifiers>0,"When passing arguments to sprintf, you must also specify their respective specifiers.");
		//the code below must be wrapped in if constexpr -- or otherwise it will spam GCC errors.
		if constexpr(number_of_specifiers>0)
		{
			constexpr auto specifiers = format_str_parse<formatstr>();

			constexpr formatstr_info info = specifiers[number];

			//constexpr auto format = substr_by_position(formatstr,info.begin+1,info.end-1);
			//constexpr char* format = formatstr+info.begin+1;

			//static_assert(format[0]=='d');

			//constexpr char* x = format.data();

			std::string result;
			constexpr auto formatstr_text_chunk = substr_by_position( formatstr, (number == 0)? 0 : specifiers[number-1].format.end+1,  info.format.begin-1);
			result += formatstr_text_chunk;
				
			result += sprintf_format_handler<formatstr,info.format.begin+1,info.format.end-1>(std::get<number>(std::make_tuple(args...)));

			if constexpr (number+1 == specifiers.size())
			{
				//add on the rest of the format string to the end.
				result += formatstr+info.format.end+1;
			}
			else
			{
				//otherwise keep sprintf'ing till we're done
				result += sprintf<number+1,formatstr>(args...);
			}

			return std::move(result);
		}
	}
	template<const char* formatstr>
	inline auto sprintf()
	{
		return std::string(formatstr);
	}
	template<const char* formatstr, typename ...T>
	inline auto sprintf(T ... args)
	{
		return sprintf<0,formatstr>(args...);
	}
	
	template<const char* format, typename ... T>
	inline void printf(T ... args)
	{
		auto result = string_manip::sprintf<format>(args...);
		std::fwrite(result.data(),sizeof(char),result.length(),stdout);
	}

}
