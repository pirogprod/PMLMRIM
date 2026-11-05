#include "mrim_session.h"
#include <cassert>
#include <iostream>
int main(){using namespace pmlmrim;FileOffer f;f.peer="a@example.org";f.id=1234;f.totalSize=10;f.files="file.txt;10;";f.addresses="127.0.0.1:12345;";auto raw=makeFileOffer(19,f);Packet p;assert(decodeOne(raw,p));assert(raw.empty());auto got=parseFileOffer(p);assert(got.peer==f.peer&&got.id==f.id&&got.files==f.files&&got.addresses==f.addresses);FileOfferAck a;a.peer=f.peer;a.id=f.id;a.status=1;a.addresses=f.addresses;raw=makeFileOfferAck(20,a);assert(decodeOne(raw,p));auto b=parseFileOfferAck(p);assert(b.status==1&&b.id==1234); p.body.pop_back();bool threw=false;try{parseFileOfferAck(p);}catch(const std::exception&){threw=true;}assert(threw);std::cout<<"MRIM file codec tests passed\n";}
