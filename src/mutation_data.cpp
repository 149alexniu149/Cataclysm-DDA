#include "mutation.h"
#include "json.h"
#include "pldata.h" // traits
#include "enums.h" // tripoint
#include "bodypart.h"
#include "debug.h"
#include "martialarts.h"

#include <vector>
#include <map>

std::vector<dream> dreams;
std::map<std::string, std::vector<std::string> > mutations_category;
std::unordered_map<std::string, mutation_branch> mutation_data;

static void extract_mod(JsonObject &j, std::unordered_map<std::pair<bool, std::string>, int> &data,
                        std::string mod_type, bool active, std::string type_key)
{
    int val = j.get_int(mod_type, 0);
    if (val != 0) {
        data[std::make_pair(active, type_key)] = val;
    }
}

static void load_mutation_mods(JsonObject &jsobj, std::string member, std::unordered_map<std::pair<bool, std::string>, int> &mods)
{
    if (jsobj.has_object(member)) {
        JsonObject j = jsobj.get_object(member);
        bool active = false;
        if (member == "active_mods") {
            active = true;
        }
        //                   json field             type key
        extract_mod(j, mods, "str_mod",     active, "STR");
        extract_mod(j, mods, "dex_mod",     active, "DEX");
        extract_mod(j, mods, "per_mod",     active, "PER");
        extract_mod(j, mods, "int_mod",     active, "INT");
    }   
}

void mutation_branch::load( JsonObject &jsobj )
{
    const std::string id = jsobj.get_string( "id" );
    mutation_branch &new_mut = mutation_data[id];

    JsonArray jsarr;
    new_mut.name = _(jsobj.get_string("name").c_str());
    new_mut.description = _(jsobj.get_string("description").c_str());
    new_mut.points = jsobj.get_int("points");
    new_mut.visibility = jsobj.get_int("visibility", 0);
    new_mut.ugliness = jsobj.get_int("ugliness", 0);
    new_mut.startingtrait = jsobj.get_bool("starting_trait", false);
    new_mut.mixed_effect = jsobj.get_bool("mixed_effect", false);
    new_mut.activated = jsobj.get_bool("active", false);
    new_mut.cost = jsobj.get_int("cost", 0);
    new_mut.cooldown = jsobj.get_int("time",0);
    new_mut.hunger = jsobj.get_bool("hunger",false);
    new_mut.thirst = jsobj.get_bool("thirst",false);
    new_mut.fatigue = jsobj.get_bool("fatigue",false);
    new_mut.valid = jsobj.get_bool("valid", true);
    new_mut.purifiable = jsobj.get_bool("purifiable", true);
    new_mut.initial_ma_styles = jsobj.get_string_array( "initial_ma_styles" );
    new_mut.threshold = jsobj.get_bool("threshold", false);
    new_mut.profession = jsobj.get_bool("profession", false);
    
    load_mutation_mods(jsobj, "passive_mods", new_mut.mods);
    /* Not currently supported due to inability to save active mutation state
    load_mutation_mods(jsobj, "active_mods", new_mut.mods); */

    new_mut.prereqs = jsobj.get_string_array( "prereqs" );
    // Helps to be able to have a trait require more than one other trait
    // (Individual prereq-lists are "OR", not "AND".)
    // Traits shoud NOT appear in both lists for a given mutation, unless
    // you want that trait to satisfy both requirements.
    // These are additional to the first list.
    new_mut.prereqs2 = jsobj.get_string_array( "prereqs2" );
    // Dedicated-purpose prereq slot for Threshold mutations
    // Stuff like Huge might fit in more than one mutcat post-threshold, so yeah
    new_mut.threshreq = jsobj.get_string_array( "threshreq" );
    new_mut.cancels = jsobj.get_string_array( "cancels" );
    new_mut.replacements = jsobj.get_string_array( "changes_to" );
    new_mut.additions = jsobj.get_string_array( "leads_to" );
    jsarr = jsobj.get_array("category");
    while (jsarr.has_more()) {
        std::string s = jsarr.next_string();
        new_mut.category.push_back(s);
        mutations_category[s].push_back(id);
    }
    jsarr = jsobj.get_array("wet_protection");
    while (jsarr.has_more()) {
        JsonObject jo = jsarr.next_object();
        std::string part_id = jo.get_string("part");
        int ignored = jo.get_int("ignored", 0);
        int neutral = jo.get_int("neutral", 0);
        int good = jo.get_int("good", 0);
        tripoint protect = tripoint(ignored, neutral, good);
        new_mut.protection[part_id] = mutation_wet(body_parts[part_id], protect);
    }
}

static void check_consistency( const std::vector<std::string> &mvec, const std::string &mid, const std::string &what )
{
    for( const auto &m : mvec ) {
        if( !mutation_branch::has( m ) ) {
            debugmsg( "mutation %s refers to undefined %s %s", mid.c_str(), what.c_str(), m.c_str() );
        }
    }
}

void mutation_branch::check_consistency()
{
    for( const auto & m : mutation_data ) {
        const auto &mid = m.first;
        const auto &mdata = m.second;
        for( const auto & style : mdata.initial_ma_styles ) {
            if( martialarts.count( style ) == 0 ) {
                debugmsg( "mutation %s refers to undefined martial art style %s", mid.c_str(), style.c_str() );
            }
        }
        ::check_consistency( mdata.prereqs, mid, "prereq" );
        ::check_consistency( mdata.prereqs2, mid, "prereqs2" );
        ::check_consistency( mdata.threshreq, mid, "threshreq" );
        ::check_consistency( mdata.cancels, mid, "cancels" );
        ::check_consistency( mdata.replacements, mid, "replacements" );
        ::check_consistency( mdata.additions, mid, "additions" );
    }
}

nc_color mutation_branch::get_display_color() const
{
    if( threshold || profession ) {
        return c_white;
    } else if( mixed_effect ) {
        return c_pink;
    } else if( points > 0 ) {
        return c_ltgreen;
    } else if( points < 0 ) {
        return c_ltred;
    } else {
        return c_yellow;
    }
}

bool mutation_branch::has( const std::string &mutation_id )
{
    return mutation_data.count( mutation_id ) > 0;
}

const mutation_branch &mutation_branch::get( const std::string &mutation_id )
{
    const auto iter = mutation_data.find( mutation_id );
    if( iter != mutation_data.end() ) {
        return iter->second;
    }
    debugmsg( "Requested unknown mutation %s", mutation_id.c_str() );
    static mutation_branch dummy;
    return dummy;
}

const std::string &mutation_branch::get_name( const std::string &mutation_id )
{
    return get( mutation_id ).name;
}

const mutation_branch::MutationMap &mutation_branch::get_all()
{
    return mutation_data;
}

void mutation_branch::reset_all()
{
    mutations_category.clear();
    mutation_data.clear();
}

void load_dream(JsonObject &jsobj)
{
    dream newdream;

    newdream.strength = jsobj.get_int("strength");
    newdream.category = jsobj.get_string("category");

    JsonArray jsarr = jsobj.get_array("messages");
    while (jsarr.has_more()) {
        newdream.messages.push_back(_(jsarr.next_string().c_str()));
    }

    dreams.push_back(newdream);
}

bool trait_display_sort( const std::string &a, const std::string &b ) noexcept
{
    return mutation_data[a].name < mutation_data[b].name;
}
