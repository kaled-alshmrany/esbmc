#include <iostream>
#include <cassert>
#include <set>
using namespace std;

int main ()
{
  multiset<int> myset;
  multiset<int>::value_compare mycomp;
  multiset<int>::iterator it;
  int i,highest;

  mycomp = myset.value_comp();

  for (i=0; i<=5; i++) myset.insert(i);

  cout << "myset contains:";

  highest=*myset.rbegin();
  assert(highest == 5);
  it=myset.begin();
  assert(*it == 0);
  do {
    cout << " " << *it;
  } while ( mycomp(*it++,highest) );

  cout << endl;

  return 0;
}
