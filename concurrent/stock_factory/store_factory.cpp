#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

class Stock{
public:
    Stock(const std::string & name) : key_(name) {}

    const std::string& key() {
        return key_;
    }

    std::string key_;
};

class StockFactory : public std::enable_shared_from_this<StockFactory>
{
public:
    std::shared_ptr<Stock> get(const std::string& key) {
        std::shared_ptr<Stock> pStock;
        std::lock_guard<std::mutex> lock(mutex_);
        std::weak_ptr<Stock>& wkStock = stocks_[key];
        pStock = wkStock.lock();
        if (!pStock) {
            pStock.reset(new Stock(key),
                std::bind(&StockFactory::weakDeleteCallback, 
                    std::weak_ptr<StockFactory>(shared_from_this()), 
                    std::placeholders::_1));
                wkStock = pStock;
        }
        return pStock;
    }

private:
    static void weakDeleteCallback(const std::weak_ptr<StockFactory>& wkFactory, Stock* stock){
        std::shared_ptr<StockFactory> factory(wkFactory.lock());
        if (factory) {
            factory->removeStock(stock);
        }
        delete stock;
    }

    void removeStock(Stock* stock) {
        if (stock) {
            std::lock_guard<std::mutex> lock(mutex_);
            stocks_.erase(stock->key());
        }
    }

    mutable std::mutex mutex_;
    std::map<std::string, std::weak_ptr<Stock> > stocks_;
};
 

int main() {

}