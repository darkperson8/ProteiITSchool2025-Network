#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <vector>
#include <string>

//Разбивка выражения на лексемы (скобки, базовые операторы, целые числа)
std::vector<std::string> getTokens(const std::string& exp);
//Алгоритм сортировочной станции (из инфиксной нотации в обратную польскую)
std::vector<std::string> infixToRPN(const std::string& exp);
//Вычисление выражения в обратной польской нотации
double calc(const std::vector<std::string>& tokens);

#endif //CALCULATOR_H
