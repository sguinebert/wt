/* last include in main.cpp  */
#pragma once

#include <Wt/WWidget.h>
#include "reflection.hpp"

namespace Wt {
void WWidget::declare() {
    dispatch(*this, [&]<class T>(T& obj) {
        std::cout << "Animal render for " << std::meta::identifier_of(^^T) << '\n';
        attributes.emplace("rendered", std::string(std::meta::identifier_of(^^T)));
    });
}

// void registerWidget(DomElement& domElement){
// #ifdef USE_CPP26_REFLECTION
//     Wt::cpp26::dispatch(*this, []template <class T>(T& args) {
//         std::cout << "In global namespace: " << std::meta::identifier_of(^^T) << std::endl;
//         domElement.setAttribute("data-wt", std::meta::identifier_of(^^T));
//     });
// #endif
// }
// void registerWidget(WWidget* widget){
// #ifdef USE_CPP26_REFLECTION
//     Wt::cpp26::dispatch(*this, []template <class T>(T& args) {
//         std::cout << "In global namespace: " << std::meta::identifier_of(^^T) << std::endl;
//         domElement.setAttribute("data-wt", std::meta::identifier_of(^^T));
//     });
// #endif
// }
}

namespace glz {
template <class T>
requires auto_glaze<T, Wt::WWidget>
struct meta<T> {
    static constexpr auto value = make_object_for_type<T>();
};
}
