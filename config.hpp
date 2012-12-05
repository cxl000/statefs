#ifndef _STATEFS_CONFIG_HPP_
#define _STATEFS_CONFIG_HPP_

#include <cor/notlisp.hpp>
#include <cor/options.hpp>
#include <cor/sexp.hpp>

#include <boost/variant.hpp>
#include <boost/filesystem.hpp>


namespace config
{

namespace nl = cor::notlisp;

typedef boost::variant<long, double, std::string> property_type;

void to_property(nl::expr_ptr expr, property_type &dst);
std::string to_string(property_type const &p);

struct AnyToString : public boost::static_visitor<>
{
    std::string &dst;

    AnyToString(std::string &res) : dst(res) {}

    void operator () (std::string const &v) const
    {
        dst = v;
    }

    template <typename T>
    void operator () (T &v) const
    {
        std::stringstream ss;
        ss << v;
        dst = ss.str();
    }
};

long to_integer(property_type const &src);

class Property : public nl::ObjectExpr
{
public:
    enum Access
    {
        Read = 1,
        Write = 2,
        Subscribe = 4
    };

    Property(std::string const &name,
             property_type const &defval,
             unsigned access = Read)
        : ObjectExpr(name), defval_(defval), access_(access)
    {}

    std::string defval() const;

    unsigned access() const
    {
        return access_;
    }

    int mode(int umask = 0022) const;

private:
    property_type defval_;
    unsigned access_;
};

class Namespace : public nl::ObjectExpr
{
public:
    typedef std::shared_ptr<Property> prop_type;
    typedef std::list<prop_type> storage_type;
    Namespace(std::string const &name, storage_type &&props)
        : ObjectExpr(name), props_(props)
    {}

    storage_type props_;
};

class Plugin : public nl::ObjectExpr
{
public:
    typedef std::shared_ptr<Namespace> ns_type;
    typedef std::list<ns_type> storage_type;
    Plugin(std::string const &name, std::string const &path,
           storage_type &&namespaces)
        : ObjectExpr(name), path(path), namespaces_(namespaces)
    {}

    std::string path;
    storage_type namespaces_;
};

nl::env_ptr mk_parse_env();

template <typename CharT, typename ReceiverT>
void parse(std::basic_istream<CharT> &input, ReceiverT receiver)
{
    using namespace nl;

    env_ptr env(mk_parse_env());

    Interpreter config(env);
    cor::error_tracer([&input, &config]()
                      { cor::sexp::parse(input, config); });

    ListAccessor res(config.results());
    rest_casted<Plugin>(res, receiver);
}

template <typename ReceiverT>
void from_file(std::string const &cfg_src, ReceiverT receiver)
{
    trace() << "Loading config from " << cfg_src << std::endl;
    try {
        std::ifstream input(cfg_src);
        parse(input, receiver);
    } catch (...) {
        std::cerr << "Error parsing " << cfg_src << ", skiping..."
                  << std::endl;
    }
}

namespace fs = boost::filesystem;

template <typename ReceiverT>
void from_dir(std::string const &cfg_src, ReceiverT receiver)
{
    trace() << "Config dir " << cfg_src << std::endl;
    std::for_each(fs::directory_iterator(cfg_src),
                  fs::directory_iterator(),
                  [&receiver](fs::directory_entry const &d) {
                      if (d.path().extension() == ".scm")
                          from_file(d.path().string(), receiver);
                  });
}

template <typename ReceiverT>
void load(char const *cfg_src, ReceiverT receiver)
{
    if (!cfg_src)
        return;

    if (fs::is_regular_file(cfg_src))
        return from_file(cfg_src, receiver);

    if (fs::is_directory(cfg_src))
        return from_dir(cfg_src, receiver);

    throw cor::Error("Unknown configuration source %s", cfg_src);
}

} // config

#endif // _STATEFS_CONFIG_HPP_
