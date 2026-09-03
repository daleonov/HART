#pragma once

#include <map>
#include <ostream>
#include <sstream>
#include <string>

#include "hart_exceptions.hpp"
#include "hart_stringify.hpp"

// TODO: Document this macro
#define HART_CAPTURE_VALUE(expression) ::hart::ActiveCapturedValuesContext::get().capture (#expression, expression)

namespace hart
{

class CapturedValuesContext
{
public:
    template <typename ValueType>
    void capture (const char* token, const ValueType& value)
    {
        m_capturedValues[token] = hart::toString (value);
    }

    void clear()
    {
        m_capturedValues.clear();
    }

    bool isEmpty() const
    {
        return m_capturedValues.empty();
    }

    std::string toString() const
    {
        std::ostringstream stream;
        represent (stream);
        return stream.str();
    }

    void represent (std::ostream& stream) const
    {
        for (const auto& capturedValue : m_capturedValues)
            stream << capturedValue.first << " = " << capturedValue.second << '\n';
    }

private:
    // Tokens will be stored in lexicographical order here,
    // although unordered_map can suffice here too
    std::map<std::string, std::string> m_capturedValues;
};

class ActiveCapturedValuesContext
{
public:
    static CapturedValuesContext& get()
    {
        if (currentContext == nullptr)
        {
            static CapturedValuesContext fallbackContext;

            HART_THROW_OR_RETURN (
                hart::ConfigurationError,
                "HART_CAPTURE_VALUE() can only be used inside a HART task",
                fallbackContext
            );
        }

        return *currentContext;
    }

    static bool hasActiveContext()
    {
        return currentContext != nullptr;
    }

    static thread_local CapturedValuesContext* currentContext;
};

class CapturedValuesContextScope
{
public:
    explicit CapturedValuesContextScope (CapturedValuesContext& context)
    {
        m_previousContext = ActiveCapturedValuesContext::currentContext;
        ActiveCapturedValuesContext::currentContext = &context;
    }

    ~CapturedValuesContextScope()
    {
        ActiveCapturedValuesContext::currentContext = m_previousContext;
    }

private:
    CapturedValuesContext* m_previousContext = nullptr;
};

#if defined (HART_IMPLEMENTATION)
thread_local CapturedValuesContext* ActiveCapturedValuesContext::currentContext = nullptr;
#endif

}  // namespace hart
