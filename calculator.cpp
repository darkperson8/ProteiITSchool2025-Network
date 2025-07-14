#include "calculator.h"

#include <sstream>
#include <stack>
#include <unordered_map>

std::unordered_map<std::string, unsigned int> priorities{ {"*", 1}, {"/", 1}, {"+", 2}, {"-", 2} };

std::vector<std::string> getTokens(const std::string& exp)
{
    std::vector<std::string> res;
    std::stringstream ss(exp);
    char symbol;
    int number;
    while (ss.get(symbol))
    {
        switch (symbol) {
            case '+':
            case '*':
            case '/':
            case '(':
            case ')':
            {
                res.emplace_back(1, char(symbol));
                break;
            }
            case '-':
            {
                if (ss.get(symbol))
                {
                    if (std::isdigit(symbol) && !std::isdigit(res.back().back()) && res.back() != ")")
                    {
                        ss.unget();
                        ss.unget();
                        ss >> number;
                        res.push_back(std::to_string(number));
                    }
                    else
                    {
                        ss.unget();
                        res.emplace_back("-");
                    }
                }
                else
                    res.clear();
                break;
            }
            default:
            {
                if (std::isdigit(symbol))
                {
                    ss.unget();
                    ss >> number;
                    res.push_back(std::to_string(number));
                }
                else
                {
                    res.clear();
                    ss.setstate(std::ios::failbit);
                }
            }
        }
    }
    return res;
}

std::vector<std::string> infixToRPN(const std::string& exp)
{
    std::vector<std::string> tokens = getTokens(exp);
    std::vector<std::string> result;
    std::stack<std::string> opStack;
    for (auto const& token : tokens)
    {
        if (std::isdigit(token.back()))
        {
            result.push_back(token);
        }
        else if (token == "(")
            opStack.push(token);
        else if (token == ")")
        {
            while (!opStack.empty() && opStack.top() != "(")
            {
                result.push_back(opStack.top());
                opStack.pop();
            }
            if (opStack.empty())
            {
                result.clear();
                break;
            }
            else
                opStack.pop();
        }
        else
        {
            int priority = priorities[token];
            while (!opStack.empty() && opStack.top() != "(" && priorities[opStack.top()] <= priority)
            {
                result.push_back(opStack.top());
                opStack.pop();
            }
            opStack.push(token);
        }
    }
    while (!opStack.empty())
    {
        result.push_back(opStack.top());
        opStack.pop();
    }
    return result;
}

double calc(const std::vector<std::string>& tokens)
{
    std::stack<double> argsStack;
    for (auto const& token : tokens)
    {
        if (std::isdigit(token.back()))
        {
            argsStack.push(std::stod(token));
        }
        else
        {
            double arg2 = argsStack.top();
            argsStack.pop();
            double arg1 = argsStack.top();
            argsStack.pop();
            switch (token[0])
            {
            case '+':
                argsStack.push(arg1 + arg2);
                break;
            case '-':
                argsStack.push(arg1 - arg2);
                break;
            case '*':
                argsStack.push(arg1 * arg2);
                break;
            case '/':
                argsStack.push(arg1 / arg2);
                break;
            }
        }
    }
    return argsStack.top();
}