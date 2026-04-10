#pragma once

#include <ostream>

#include "hart_condition.hpp"

namespace hart
{

/// @brief Holds metadata for the condition + the expect/assert level and file/line metadata
/// @details Meant to add strictness (expect/assert) to the condition, for free-standing assertions like `HART_EXPECT_EQ`.
/// @private
class Assertion
{
public:
    enum class Level
    {
        expect,
        assert
    };

    static Assertion makeExpect (Condition condition, const char* file, int line)
    {
        return Assertion (Level::expect, std::move (condition), file, line);
    }

    static Assertion makeAssert (Condition condition, const char* file, int line)
    {
        return Assertion (Level::assert, std::move (condition), file, line);
    }

    bool hasFailed() const
    {
        return m_condition.getResult() == false;
    }

    Level getLevel() const
    {
        return m_level;
    }

    const char* getFile() const
    {
        return m_file;
    }

    int getLine() const
    {
        return m_line;
    }

    void representWithTokens (std::ostream& stream) const
    {
        m_condition.representWithTokens (stream);
    }

    void representWithStringRepresentations (std::ostream& stream) const
    {
        m_condition.representWithStringRepresentations (stream);
    }

private:
    Assertion (Level level, Condition condition, const char* file, int line) :
        m_level (level),
        m_condition (std::move (condition)),
        m_file (file),
        m_line (line)
    {
    }

    const Level m_level;
    const Condition m_condition;
    const char* const m_file;
    const int m_line;
};

}  // namespace hart
