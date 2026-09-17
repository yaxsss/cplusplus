#include <iostream>

class Observable;

class Observer {
    public:
    void observe(Observable * s);
    ~Observer();
    Observable* subject_;
    void update();
};


class Observable {
    public:
    void register_(Observer *x);
    void unregister(Observer *x);
    void notifyObservers() {
        for (Observer* x : observers_) {
            x->update();
        }
    }
    private:
    std::vector<Observer*> observers_;
};

void Observer::observe(Observable * s) {
    s->register_(this);
    subject_ = s;
}
Observer::~Observer() {
    subject_->unregister(this);
}

