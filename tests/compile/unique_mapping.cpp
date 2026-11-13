#include <CaliburnMicroQt/IInstanceProvider.h>
struct Plain { int value = 0; };
struct Base {
#ifdef SAFE_MAPPING
    virtual ~Base() = default;
#endif
};
struct Derived : Base {};
void checkMapping()
{
    auto concrete = UniqueInstance::from<Plain>(std::make_unique<Plain>());
    auto interface = UniqueInstance::from<Base>(std::make_unique<Derived>());
}
