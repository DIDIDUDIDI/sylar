#include "person.h"
#include <iostream>

namespace DIDI {
    Person::Person(int age) : m_age(age) {}
    void Person::checkAge() {
         std::cout << m_age << std::endl;
    }
}