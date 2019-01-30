#include "flag_interpreter.hpp"


namespace flag_interpreter
{
	results_t process(int argument_count, char const* const arguments[], shorthands_t shorthands)
	{
		results_t returnval;
		for (int argid=0;argid<argument_count;argid++)
		{
			std::string arg = std::string(arguments[argid]);
			if (arg[0] == '-')
			{
				std::string arg_no_dash;
				std::string arg_full_name;
				if (arg[1] == '-')
				{
					arg_no_dash ,arg_full_name = arg.substr(2);
				}
				else
				{
					arg_no_dash = arg_full_name = arg.substr(1);
				}
				//strip =value off the full name
				auto equals_seperated = string_manip::get_before_and_after_sequence(arg_full_name,"=");
				if (equals_seperated.has_value())
				{
					arg_full_name = std::get<0>(equals_seperated.value());
				}

				if (!(arg[1] == '-'))
				{
					auto iterator = shorthands.find(arg_full_name);
					if (iterator != shorthands.end())
					{
						arg_full_name = iterator->second;
					}
					else
					{
						throw invalid_shorthand(arg_full_name.c_str());
					}
				}

				//if there was a --var="value" then add to returnval
				if (equals_seperated.has_value())
				{
					returnval.push_back({arg_full_name,string_manip::seperate_comma_seperated_string(std::get<1>(equals_seperated.value()))});
					continue;
				}

				int next_argid = argid+1;
				if (next_argid == argument_count)//if this is the last word and no equals value then there is no argument to this flag
				{
					returnval.push_back({arg_full_name,{}});
				}
				else 
				{
					if (arguments[next_argid][0] != '-')
					{
						returnval.push_back({arg_full_name,string_manip::seperate_comma_seperated_string(std::string(arguments[next_argid]))});
					}
					else
					{
						returnval.push_back({arg_full_name,{}});
					}
				}
			}
		}
		return returnval;
	}
	




	results_t easy_test_flag_interpreter(std::vector<std::string> _args, shorthands_t shorthands)
	{
		std::vector<const char*> args = {};
		for (auto& s : _args)
		{
			args.push_back(s.c_str());
		}
		return process(args.size(),args.data(),shorthands);
	}
	auto shorthands = flag_interpreter::shorthands_t{
		{"t","test"}
	};

}
