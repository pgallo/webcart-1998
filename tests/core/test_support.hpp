#pragma once

#include <memory>
#include <string>

#include "webcart/shop.hpp"

namespace webcart::test {

inline Catalog data_catalog() { return Catalog::load_csv(std::string(WEBCART_DATA_DIR) + "/catalog.csv"); }

inline std::vector<Promotion> data_promotions() {
    return load_promotions_csv(std::string(WEBCART_DATA_DIR) + "/promotions.csv");
}

inline std::unique_ptr<Shop> make_shop(std::shared_ptr<ManualClock> clock, std::string journal = "") {
    ShopConfig cfg;
    cfg.id_seed = 42;
    cfg.journal_path = std::move(journal);
    return std::make_unique<Shop>(data_catalog(), data_promotions(), cfg, std::move(clock));
}

}  // namespace webcart::test
