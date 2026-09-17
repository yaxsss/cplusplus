#include <iostream>
#include <vector>
#include <functional>


template <typename Signature>
class SignalTrivial;

template<typename RET, typename... ARGS>
class SignalTrivial<RET(ARGS...)> {
public:
    typedef std::function<RET(ARGS...)> Functor;
    typedef std::vector<Functor> FunctorList;

    void connect(Functor&& func) {
        functors_.push_back(func);
    }

    void disconnect(Functor&& func) {
        functors_.erase(std::remove(functors_.begin(), functors_.end(), func), functors_.end());

};