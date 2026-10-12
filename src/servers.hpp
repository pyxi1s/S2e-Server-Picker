#pragma once

#include <string>
#include <vector>

namespace dsp {

struct Region {
    std::string id;
    std::string group;
    std::string name;
    std::vector<std::string> cidrs;
    bool placeholder;
};

inline const std::vector<Region>& GetRegions() {
    static const std::vector<Region> regions = {
        {"sgp", "Southeast Asia(SEA)", "Singapore(SG)",
         {
          "192.0.2.0/28",
         }, true},
        {"bkk", "Southeast Asia(SEA)", "Bangkok(BKK)",
         {
          "192.0.2.0/28",
         }, true},
        {"hkg", "East Asia(EA)", "Hong Kong(HK)",
         {
          "192.0.2.0/28",
         }, true},
        {"tyo", "East Asia(EA)", "Tokyo(TYO)",
         {
          "192.0.2.0/28",
         }, true},
        {"seo", "East Asia(EA)", "Seoul(SEL)",
         {
          "192.0.2.0/28",
         }, true},
        {"iad", "North America(NA)", "Washington D.C.(IAD)",
         {
          "192.0.2.0/28",
         }, true},
        {"atl", "North America(NA)", "Atlanta(ATL)",
         {
          "192.0.2.0/28",
         }, true},
        {"mia", "North America(NA)", "Miami(MIA)",
         {
          "192.0.2.0/28",
         }, true},
        {"ord", "North America(NA)", "Chicago(ORD)",
         {
          "192.0.2.0/28",
         }, true},
        {"dfw", "North America(NA)", "Dallas(DFW)",
         {
          "192.0.2.0/28",
         }, true},
        {"den", "North America(NA)", "Denver(DEN)",
         {
          "192.0.2.0/28",
         }, true},
        {"lax", "North America(NA)", "Los Angeles(LAX)",
         {
          "192.0.2.0/28",
         }, true},
        {"sjc", "North America(NA)", "San Jose(SJC)",
         {
          "192.0.2.0/28",
         }, true},
        {"sea", "North America(NA)", "Seattle(SEA)",
         {
          "192.0.2.0/28",
         }, true},
        {"yyz", "North America(NA)", "Toronto(YYZ)",
         {
          "192.0.2.0/28",
         }, true},
        {"fra", "Europe(EU)", "Frankfurt(FRA)",
         {
          "192.0.2.0/28",
         }, true},
        {"ams", "Europe(EU)", "Amsterdam(AMS)",
         {
          "192.0.2.0/28",
         }, true},
        {"lhr", "Europe(EU)", "London(LHR)",
         {
          "192.0.2.0/28",
         }, true},
        {"par", "Europe(EU)", "Paris(PAR)",
         {
          "192.0.2.0/28",
         }, true},
        {"mad", "Europe(EU)", "Madrid(MAD)",
         {
          "192.0.2.0/28",
         }, true},
        {"mil", "Europe(EU)", "Milan(MIL)",
         {
          "192.0.2.0/28",
         }, true},
        {"vie", "Europe(EU)", "Vienna(VIE)",
         {
          "192.0.2.0/28",
         }, true},
        {"waw", "Europe(EU)", "Warsaw(WAW)",
         {
          "192.0.2.0/28",
         }, true},
        {"sto", "Europe(EU)", "Stockholm(STO)",
         {
          "192.0.2.0/28",
         }, true},
        {"hel", "Europe(EU)", "Helsinki(HEL)",
         {
          "192.0.2.0/28",
         }, true},
        {"gru", "South America(SA)", "Sao Paulo(GRU)",
         {
          "192.0.2.0/28",
         }, true},
        {"scl", "South America(SA)", "Santiago(SCL)",
         {
          "192.0.2.0/28",
         }, true},
        {"lim", "South America(SA)", "Lima(LIM)",
         {
          "192.0.2.0/28",
         }, true},
        {"eze", "South America(SA)", "Buenos Aires(EZE)",
         {
          "192.0.2.0/28",
         }, true},
        {"bog", "South America(SA)", "Bogota(BOG)",
         {
          "192.0.2.0/28",
         }, true},
        {"syd", "Oceania(OCE)", "Sydney(SYD)",
         {
          "192.0.2.0/28",
         }, true},
        {"mel", "Oceania(OCE)", "Melbourne(MEL)",
         {
          "192.0.2.0/28",
         }, true},
        {"akl", "Oceania(OCE)", "Auckland(AKL)",
         {
          "192.0.2.0/28",
         }, true},
    };
    return regions;
}

}
