#include <QtTest>
#include <ossia/ossia.hpp>
#include <iostream>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/network/base/address.hpp>
using namespace ossia;
namespace ossia
{

class OSSIA_EXPORT functional_state
{
public:
  std::vector<ossia::Destination> inputs;
  std::vector<ossia::Destination> outputs;
  int priority = 0;

  std::function<void (std::vector<ossia::value_with_unit>&)> func;

  void launch() const
  {
    // 1. Get the value of the first ones, maybe with a pull ?
    std::vector<ossia::value_with_unit> vals;

    vals.reserve(inputs.size());
    for(auto& dest : inputs)
    {
      vals.push_back(ossia::net::get_value(dest));
    }

    // 2. Apply
    if(func)
      func(vals);

    // 3. Push the values of the last one.
    auto n_min = std::min(vals.size(), outputs.size());

    for(int i = 0 ; i < n_min ; i++)
    {
      ossia::net::push_value(outputs[i], vals[i]);
    }

  }

  friend bool operator==(const functional_state& lhs, const functional_state& rhs)
  {
    return false;
  }
  friend bool operator!=(const functional_state& lhs, const functional_state& rhs)
  {
    return true;
  }
};


class OSSIA_EXPORT functional_state_composition
{
public:
  std::vector<functional_state> call_chain;
  // priority ? // priority relative to messages ?


  void launch() const
  {
    if(call_chain.size() > 0)
    {
      // 1. Get the value of the first ones, maybe with a pull ?
      std::vector<ossia::value_with_unit> vals;
      auto& inputs = call_chain.front().inputs;
      vals.reserve(inputs.size());
      for(auto& dest : inputs)
      {
        vals.push_back(ossia::net::get_value(dest));
      }

      // 2. Apply recursively
      for(auto& e : call_chain)
      {
        if(e.func)
          e.func(vals);
      }

      // 3. Push the values of the last one.
      auto& outputs = call_chain.back().outputs;
      auto n_min = std::min(vals.size(), outputs.size());

      for(int i = 0 ; i < n_min ; i++)
      {
        ossia::net::push_value(outputs[i], vals[i]);
      }

    }
  }
};
}

struct mock_autom2
{
  ossia::Destination mDestination;
  ossia::unit_t mUnit;
  ossia::functional_state state()
  {
    ossia::functional_state autom_h;

    // For an autom, same input & output
    autom_h.inputs.push_back(mDestination);
    autom_h.outputs.push_back(mDestination);
    autom_h.func = [=] (std::vector<ossia::value_with_unit>& v)
    {
      assert(v.size() == 1);
      auto& s = v[0];

      // destination_index ? how to say that we change h ? [0] + hsv...

      // 2. potentially convert value to the automation dataspace
      // e.g. if the automation changes h in hsv, we convert s to hsv
      std::cerr << "before conversion: " << to_pretty_string(s) << std::endl;
      if(mUnit)
      {
        s = convert(s, mUnit);
      }
      std::cerr << "after conversion: " << to_pretty_string(s) << std::endl;

      // 3. merge the automation result in s
      s = merge(s, computeValue(), mDestination.index);
      std::cerr << "after merge: " << to_pretty_string(s) << std::endl;


    };

    return autom_h;
  }

  ossia::Float computeValue() { return 0.8; }
};

using namespace ossia::net;
class StateTest : public QObject
{
  Q_OBJECT

private Q_SLOTS:

  void test_convert()
  {
    auto res = ossia::convert(ossia::centimeter(5),  ossia::millimeter_u{});

    QVERIFY(res == ossia::millimeter{50});
    QVERIFY(res != ossia::millimeter{25});
    QVERIFY(res != ossia::centimeter{5});
    convert(ossia::centimeter(5),  ossia::rgb_u{});

    qDebug() << sizeof(ossia::unit_t)
             << sizeof(ossia::value)
             << sizeof(ossia::value_with_unit)
             << sizeof(ossia::color_u)
             << sizeof(ossia::color)
             << sizeof(rgb_u)
             << sizeof(eggs::variant<rgb_u>);
  }

  void test_functional()
  {
    generic_device dev{std::make_unique<local_protocol>(), "test"};
    auto n1 = dev.createChild("n1")->createAddress(val_type::TUPLE);

    ossia::Destination d{*n1};

    // change the V of HSV
    mock_autom2 autom_v{{*n1, {2}}, ossia::hsv_u{}};
    // To change the hue, the whole value is required
    // -> extend to taking the whole value irrelevant of the destination index ?
    // -> who should handle this ?

    mock_autom2 autom_r{{*n1, {0}}, ossia::rgb_u{}};

    functional_state_composition f;
    f.call_chain.push_back(autom_v.state());
    f.call_chain.push_back(autom_r.state());

    ossia::rgb col{std::array<float, 3>{0.3, 0.4, 0.6}};
    std::vector<ossia::value_with_unit> vec; vec.push_back(col);

    std::cerr << "First: " << to_pretty_string(col) << std::endl;
    f.call_chain[0].func(vec);
    std::cerr << "2: " << to_pretty_string(vec[0]) << std::endl;
    f.call_chain[1].func(vec);
    std::cerr << "3: " << to_pretty_string(vec[0]) << std::endl;


    // We have to make a difference between the values that map to the neutral
    // unit and the ones that won't. Impossible.

    // How to handle the case where changing the hue actually changes the whole
    // color, but changing just "r" does not ?


    // Case 2 : mappings

    // Mapping 1 : a:/b[hue] -> c:/d[sat]
    // Mapping 2 : c:/d[r] -> e:/f[r]
  }

  /*! test life cycle and accessors functions */
  void test_basic()
  {
    state s;

    QCOMPARE((int32_t)s.size(), 0);

    state substate;
    s.add(substate);
    QCOMPARE((int32_t)s.size(), 1);


    state parent;
    parent.add(std::move(s));
    QCOMPARE((int32_t)parent.size(), 1);
    QCOMPARE((int32_t)s.size(), 0);

    state copy{parent};
    QCOMPARE((int32_t)copy.size(), 1);

  }

  void test_compare()
  {
    generic_device dev{std::make_unique<local_protocol>(), "test"};
    auto n1 = dev.createChild("n1")->createAddress(val_type::TUPLE);

    message m0{{*n1, {0}}, Float{5.}};
    message m1{{*n1, {0}}, Float{5.}};
    QVERIFY(m0 == m1);

    state s1, s2;
    QVERIFY(s1 == s2);

    s1.add(m0);
    QVERIFY(s1 != s2);
    QVERIFY(ossia::state_element(m0) != s1);
    QVERIFY(ossia::state_element(m0) != s2);

    s2.add(m1);
    QVERIFY(s1 == s2);

  }

  void test_remove()
  {
    generic_device dev{std::make_unique<local_protocol>(), "test"};
    auto n1 = dev.createChild("n1")->createAddress(val_type::TUPLE);

    state s;

    message m0{{*n1, {0}}, Float{5.}};
    s.add(m0);
    s.remove(m0);
    QCOMPARE((int)s.size(), 0);
  }

  void test_flatten()
  {
    generic_device dev{std::make_unique<local_protocol>(), "test"};
    auto n1 = dev.createChild("n1")->createAddress(val_type::TUPLE);

    message m0{{*n1, {0}}, Float{5.}};
    message m1{{*n1, {1}}, Float{10.}};
    message m2{{*n1, {2}}, Float{15.}};

    state s1;
    flatten_and_filter(s1, m0);
    flatten_and_filter(s1, m1);
    flatten_and_filter(s1, m2);

    QCOMPARE((int)s1.size(), 1);

    auto pw = s1.children()[0].target<piecewise_message>();
    QVERIFY(pw);
    QVERIFY(pw->address == m0.destination.value);
    Tuple expected{Float{5.}, Float{10.}, Float{15.}};
    QVERIFY(pw->value == expected);

    // permutations
    state s2;
    flatten_and_filter(s2, m2);
    flatten_and_filter(s2, m1);
    flatten_and_filter(s2, m0);

    state s3;
    flatten_and_filter(s3, m0);
    flatten_and_filter(s3, m2);
    flatten_and_filter(s3, m1);

    QVERIFY(s1 == s2);
    QVERIFY(s1 == s3);
    QVERIFY(s2 == s3);


    flatten_and_filter(s1, m0);
    QVERIFY(s1 == s2);

    // Changing a value does overwrite
    message m0_bis{{*n1, {0}}, Float{7.}};
    flatten_and_filter(s1, m0_bis);

    state_element expected_bis = piecewise_message{*n1, Tuple{Float{7.}, Float{10.}, Float{15.}}};
    QVERIFY(s1.children()[0] == expected_bis);
  }


  void test_flatten_move()
  {
    generic_device dev{std::make_unique<local_protocol>(), "test"};
    auto n1 = dev.createChild("n1")->createAddress(val_type::TUPLE);

    state_element m0 = message{{*n1, {0}}, Float{5.}};
    state_element m1 = message{{*n1, {1}}, Float{10.}};
    state_element m2 = message{{*n1, {2}}, Float{15.}};

    state s1;
    flatten_and_filter(s1, m0);
    flatten_and_filter(s1, m1);
    flatten_and_filter(s1, m2);

    QCOMPARE((int)s1.size(), 1);

    auto pw = s1.children()[0].target<piecewise_message>();
    QVERIFY(pw);
    QVERIFY(pw->address == m0.target<message>()->destination.value);
    Tuple expected{Float{5.}, Float{10.}, Float{15.}};
    QVERIFY(pw->value == expected);

    // permutations
    state s2;
    flatten_and_filter(s2, m2);
    flatten_and_filter(s2, m1);
    flatten_and_filter(s2, m0);

    state s3;
    flatten_and_filter(s3, m0);
    flatten_and_filter(s3, m2);
    flatten_and_filter(s3, m1);

    QVERIFY(s1 == s2);
    QVERIFY(s1 == s3);
    QVERIFY(s2 == s3);


    flatten_and_filter(s1, std::move(m0));
    QVERIFY(s1 == s2);

    // Changing a value does overwrite
    message m0_bis{{*n1, {0}}, Float{7.}};
    flatten_and_filter(s1, m0_bis);

    state_element expected_bis = piecewise_message{*n1, Tuple{Float{7.}, Float{10.}, Float{15.}}};
    QVERIFY(s1.children()[0] == expected_bis);
  }

  /*! test execution functions */
  void test_execution()
  {
    //! \todo test launch()
  }
};

QTEST_APPLESS_MAIN(StateTest)

#include "StateTest.moc"
