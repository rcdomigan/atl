#ifndef ATL_ABSTRACT_TYPE_HPP
#define ATL_ABSTRACT_TYPE_HPP
/**
 * @file /home/ryan/programming/atl/abstract_type.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Apr 19, 2015
 */

#include <unordered_map>
#include <unordered_set>

#include "./utility.hpp"

namespace atl
{
    typedef size_t tag_t;

    namespace abstract_type
    {
        enum SubType { abstract, concrete, function };
        struct Type;

        struct Node
        {
            enum CountType { one, at_least_one, zero_or_more };
            union
            {
                Type const* type_seq;
                tag_t type;
            };
            char tag, count;

            bool is_concrete() { return tag == concrete; }
            bool is_function() { return tag == function; }


            Node() : type(0), tag(abstract) {}

            Node(Node const& other) = default;
            Node(Type const& tt);

            constexpr Node(tag_t tt, SubType ss)
                : type(tt), tag(ss), count(one)
            {}

            // Defined in type.hpp
            std::ostream& print(std::ostream& out) const;

            void dbg() const;
        };

        namespace detail
        {
            struct MakeConcreteNode
            {
                static Node a(Type const& tt) { return Node(tt); }

                template<class T>
                static Node a(T const& tt) { return Node(tt, concrete); }
            };

            struct MakeAbstractNode
            {
                static Node a(Type const& tt) { return Node(tt); }

                template<class T>
                static Node a(T const& tt) { return Node(tt, abstract); }
            };
        }


        struct Type
        {
            typedef std::vector<Node> VecType;
            typedef VecType::const_iterator const_iterator;
            typedef VecType::iterator iterator;

            VecType values;

            Type() = default;
            Type(Type const& tt) = default;

            Type(Node const& nn) : values({nn})
            {}

            Type(tag_t tag, SubType ss)
                : values({Node(tag, ss)})
            {}

            const_iterator begin() const { return values.begin(); }
            const_iterator end() const { return values.end(); }

            // Just the values of the first node (break it off the chain of arguments).
            Node front() const
            { return values.front(); }

            Node& front()
            { return values.front(); }

            Node back() const
            { return values.back(); }

            bool is_function() const
            { return values.size() > 1; }

            size_t arity() const
            { return std::max((size_t)0, values.size() - 1); }

            bool operator==(Type const& other) const
            {
                using namespace std;
                if(other.values.size() != values.size())
                    return false;

                for(auto& vv : zip(*this, other))
                    {
                        auto& a = *get<0>(vv);
                        auto& b = *get<1>(vv);
                        if((a.tag != b.tag)
                           || ((a.tag == SubType::function)
                               && (*a.type_seq != *b.type_seq))
                           || ((a.tag != SubType::function)
                               && (a.type != b.type)))
                            return false;
                    }
                return true;
            }

            bool operator!=(Type const& other) const
            { return !(*this == other); }

            std::ostream& print(std::ostream& out) const
            {
                if(values.empty()) return out;

                if(values.size() > 1)
                    {
                        out << "(-> ";
                        front().print(out);
                        for(auto& nn : slice(*this, 1))
                            nn.print(out << " ");
                        return out << ")";
                    }
                else
                    front().print(out);

                return out;
            }

            // print contents to std::cout
            void dbg() const;
        };

        Node::Node(Type const& tt)
        {
            if(tt.values.size() == 1)
                *this = tt.values.back();
            else if(tt.values.size() > 1)
                {
                    count = CountType::one;
                    tag = function;
                    type_seq = &tt;
                }
            else
                *this = Node();
        }


        template<class Maker, class T>
        Type make_type(std::initializer_list<T> const& vec)
        {
            Type tt;
            for(auto& vv : vec)
                tt.values.push_back(Maker::a(vv));
            return tt;
        }


        template<class T>
        Type make_concrete(std::initializer_list<T> const& vec)
        { return make_type<detail::MakeConcreteNode, T>(vec); }

        template<class T>
        Type make_abstract(std::initializer_list<T> const& vec)
        { return make_type<detail::MakeAbstractNode, T>(vec); }


        void Node::dbg() const
        { print(std::cout) << std::flush; }

        void Type::dbg() const
        { print(std::cout) << std::flush; }


        template<class T>
        struct hash
        {
            typedef hash<T> Self;

            size_t operator()(T const& tt) const;
        };


        template<> struct hash<Node>
        {
            typedef hash<Node> Self;

            size_t operator()(Node const& nn) const
            {
                size_t output;
                output = nn.tag;

                if(nn.tag == SubType::function)
                    output += hash<Type>()(*nn.type_seq) << 2;
                else
                    output += nn.type << 2;

                return output;
            }
        };

        template<class T>
        size_t hash<T>::operator()(T const& tt) const
        {
            size_t values = 0;
            for(auto& nn : tt)
                values += hash<Node>()(nn);
            return values;
        }


        // I don't care about the abstract types assigned, but I care that
        // they are returned in the correct order; ie (-> a a b) and (-> c
        // c d) should be considered the same, but (-> a b a) should not
        // be.
        //
        // This differs from the operator== behaviour, which checks
        // for tag equivelence rather than layout equivelence.
        bool equivalent(Type const& a, Type const& b);
    }
}


namespace std
{
    template<>
    struct hash<atl::abstract_type::Type>
        : public atl::abstract_type::hash<atl::abstract_type::Type>
    {};

    std::ostream& operator<<(std::ostream& out,
                             const ::atl::abstract_type::Type& tt)
    { return tt.print(out); }
}

// atl::abstract_type::Type slice(atl::abstract_type::Type& tt, size_t n) { return atl::abstract_type::slice(tt, n); }
// atl::abstract_type::Type slice(atl::abstract_type::Type const& tt, size_t n) { return atl::abstract_type::slice(tt, n); }

namespace atl
{
    namespace abstract_type
    {
        // helper, see bool equivalent(...)
        bool _equivalent(std::unordered_set<Type>& have_seen_b,
                         std::unordered_map<Type, Type>& substitute,
                         Type const& a, Type const& b)
        {
            using namespace std;

            bool seen_a = substitute.count(a),
                seen_b = have_seen_b.count(b);

            if((seen_a != seen_b) || (a.values.size() != b.values.size()))
                return false;

            if(seen_a)
                return substitute[a] == b;

            else
                {
                    have_seen_b.insert(b);
                    substitute[a] = b;

                    for(auto zz : zip(a, b))
                        {
                            auto& a = *get<0>(zz);
                            auto& b = *get<1>(zz);

                            if(a.tag == SubType::function)
                                {
                                    if(a.tag != b.tag)
                                        return false;
                                    return _equivalent(have_seen_b, substitute, *a.type_seq, *b.type_seq);
                                }
                        }
                }
            return true;
        }

        bool is_equivalent(Type const& a, Type const& b)
        {
            std::unordered_set<Type> have_seen;
            std::unordered_map<Type, Type> sub;
            return _equivalent(have_seen, sub, a, b);
        }
    }
}

#endif
