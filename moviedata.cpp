#include "moviedata.hpp"
#include "command.hpp"

struct moviefile our_movie;

std::vector<char>& get_host_memory()
{
	return our_movie.host_memory;
}


namespace
{
	class get_gamename_cmd : public command
	{
	public:
		get_gamename_cmd() throw(std::bad_alloc) : command("get-gamename") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			out(win) << "Game name is '" << our_movie.gamename << "'" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Get the game name"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: get-gamename\n"
				"Prints the game name\n";
		}
	} getnamec;

	class print_authors_cmd : public command
	{
	public:
		print_authors_cmd() throw(std::bad_alloc) : command("show-authors") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			size_t idx = 0;
			for(auto i = our_movie.authors.begin(); i != our_movie.authors.end(); i++) {
				out(win) << (idx++) << ": " << i->first << "|" << i->second << std::endl;
			}
			out(win) << "End of authors list" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Show the run authors"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: show-authors\n"
				"Shows the run authors\n";
		}
	} getauthorc;

	class add_author_command : public command
	{
	public:
		add_author_command() throw(std::bad_alloc) : command("add-author") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			fieldsplitter f(t.tail());
			std::string full = f;
			std::string nick = f;
			if(full == "" && nick == "")
				throw std::runtime_error("Bad author name");
			our_movie.authors.push_back(std::make_pair(full, nick));
			out(win) << (our_movie.authors.size() - 1) << ": " << full << "|" << nick << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Add an author"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: add-author <fullname>\n"
				"Syntax: add-author |<nickname>\n"
				"Syntax: add-author <fullname>|<nickname>\n"
				"Adds a new author\n";
		}
	} addauthorc;

	class remove_author_command : public command
	{
	public:
		remove_author_command() throw(std::bad_alloc) : command("remove-author") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			uint64_t index = parse_value<uint64_t>(t.tail());
			if(index >= our_movie.authors.size())
				throw std::runtime_error("No such author");
			our_movie.authors.erase(our_movie.authors.begin() + index);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Remove an author"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: remove-author <id>\n"
				"Removes author with ID <id>\n";
		}
	} removeauthorc;

	class edit_author_command : public command
	{
	public:
		edit_author_command() throw(std::bad_alloc) : command("edit-author") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			uint64_t index = parse_value<uint64_t>(t);
			if(index >= our_movie.authors.size())
				throw std::runtime_error("No such author");
			fieldsplitter f(t.tail());
			std::string full = f;
			std::string nick = f;
			if(full == "" && nick == "") {
				out(win) << "syntax: edit-author <authornum> <author>" << std::endl;
				return;
			}
			our_movie.authors[index] = std::make_pair(full, nick);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Edit an author"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: edit-author <authorid> <fullname>\n"
				"Syntax: edit-author <authorid> |<nickname>\n"
				"Syntax: edit-author <authorid> <fullname>|<nickname>\n"
				"Edits author name\n";
		}
	} editauthorc;
}