#ifndef _fieldsplit_hpp__included__
#define _fieldsplit_hpp__included__

#include <string>
#include <stdexcept>

/**
 * \brief Class for splitting string to fields.
 *
 * Splits string to fields on |
 */
class fieldsplitter
{
public:
/**
 * \brief Create new string splitter
 *
 * Creates a new string splitter to split specified string.
 * \param _line The line to split.
 * \throws std::bad_alloc Out of memory.
 */
	fieldsplitter(const std::string& _line) throw(std::bad_alloc);
/**
 * \brief More fields coming
 *
 * Checks if more fields are coming.
 *
 * \return True if more fields are coming, otherwise false.
 */
	operator bool() throw();

/**
 * \brief Read next field
 *
 * Reads the next field and returns it. If field doesn't exist, it reads as empty string.
 *
 * \return The read field.
 * \throws std::bad_alloc
 */
	operator std::string() throw(std::bad_alloc);
private:
	std::string line;
	size_t position;
};

#endif
