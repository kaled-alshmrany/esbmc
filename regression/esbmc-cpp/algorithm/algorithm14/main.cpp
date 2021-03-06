// copy_backward example
#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
using namespace std;

int main () {
  vector<int> myvector;
  vector<int>::iterator it;
// 10 20 30 10 20 30 40 50
  // set some values:
  for (int i=1; i<=5; i++)
    myvector.push_back(i*10);          // myvector: 10 20 30 40 50

  myvector.resize(myvector.size()+3);  // allocate space for 3 more elements

  copy_backward ( myvector.begin(), myvector.begin()+5, myvector.end() );
  assert(myvector[0] == 10);
  assert(myvector[1] == 20);
  assert(myvector[2] == 30);
  assert(myvector[3] == 10);
  assert(myvector[4] == 20);
  assert(myvector[5] == 30);
  assert(myvector[6] == 40);
  assert(myvector[7] == 50);
  
  cout << "myvector contains:";
  for (it=myvector.begin(); it!=myvector.end(); ++it)
    cout << " " << *it;

  cout << endl;

  return 0;
}
