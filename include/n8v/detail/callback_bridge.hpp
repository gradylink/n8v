#pragma once

#include <n8v/n8v_c.h>

#include <deque>
#include <functional>
#include <string_view>
#include <utility>

namespace n8v::detail::callback_bridge {

template <typename Ret, typename... Args> struct CFn {
  Ret (*func)(Args..., void *);
  void *userdata;
};

template <typename Ret, typename Closure, typename... Args> CFn<Ret, Args...> makeCFnImpl(Closure *closure, Ret (Closure::*)(Args...) const) {
  return CFn<Ret, Args...>{
    [](Args... args, void *userdata) -> Ret { return (*static_cast<Closure *>(userdata))(std::forward<Args>(args)...); },
    static_cast<void *>(closure),
  };
}

template <typename Ret, typename Closure, typename... Args> CFn<Ret, Args...> makeCFnImpl(Closure *closure, Ret (Closure::*)(Args...)) {
  return CFn<Ret, Args...>{
    [](Args... args, void *userdata) -> Ret { return (*static_cast<Closure *>(userdata))(std::forward<Args>(args)...); },
    static_cast<void *>(closure),
  };
}

template <typename Closure> auto makeCFn(Closure &closure) -> decltype(makeCFnImpl(&closure, &Closure::operator())) { return makeCFnImpl(&closure, &Closure::operator()); }

inline std::deque<std::function<void()>> clickClosures;
inline std::deque<std::function<void(bool)>> boolChangeClosures;
inline std::deque<std::function<void(int)>> intChangeClosures;
inline std::deque<std::function<void(float)>> floatChangeClosures;
inline std::deque<std::function<void(std::string_view)>> textChangeClosures;

inline void clearCallbackStorage() {
  clickClosures.clear();
  boolChangeClosures.clear();
  intChangeClosures.clear();
  floatChangeClosures.clear();
  textChangeClosures.clear();
}

template <typename Ret, typename... Args, typename CFnPtr>
void bridge(std::deque<std::function<Ret(Args...)>> &storage, std::function<Ret(Args...)> fn, CFnPtr &outFn, void *&outUserdata) {
  if (!fn) {
    outFn = nullptr;
    outUserdata = nullptr;
    return;
  }
  storage.push_back(std::move(fn));
  auto c = makeCFn(storage.back());
  outFn = c.func;
  outUserdata = c.userdata;
}

inline void bridgeTextChange(std::function<void(std::string_view)> fn, n8v_text_change_fn &outFn, void *&outUserdata) {
  if (!fn) {
    outFn = nullptr;
    outUserdata = nullptr;
    return;
  }
  textChangeClosures.push_back(std::move(fn));
  outFn = [](const char *text, size_t length, void *userdata) { (*static_cast<std::function<void(std::string_view)> *>(userdata))(std::string_view(text, length)); };
  outUserdata = &textChangeClosures.back();
}

} // namespace n8v::detail::callback_bridge
