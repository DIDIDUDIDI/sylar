#pragma once
namespace DIDI {
    class Person {
    public:
        //Person() {}
        Person(int age);
        void checkAge();
    private:
        int m_age;
    };
}