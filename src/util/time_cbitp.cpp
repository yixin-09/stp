#include <ctime>
#include <vector>
#include "../AST/AST.h"
#include "../simplifier/constantBitP/FixedBits.h"
#include "../simplifier/constantBitP/MersenneTwister.h"

#include "../simplifier/constantBitP/ConstantBitP_TransferFunctions.h"
#include "../extlib-constbv/constantbv.h"

#include "../AST/ASTKind.h"
#include "../STPManager/STPManager.h"
#include "../cpp_interface/cpp_interface.h"
#include <list>

using namespace std;
using simplifier::constantBitP::FixedBits;
using namespace simplifier::constantBitP;


const int iterations = 100000;
const unsigned bitWidth = 64;

BEEV::STPMgr* beev;


class Stopwatch
{
public:
	Stopwatch() :
		start(std::clock())
	{
	}
	void stop()
	{
		clock_t total = clock() - start;
		cerr << "ticks: " << total << " " << (float(total) / CLOCKS_PER_SEC) << "s" << endl;
	}
	clock_t stop2()
	{
		clock_t total = clock() - start;
		return total;
	   //cerr.setf(ios::fixed);
//		cerr << setprecision(2)  <<  (float(total) / CLOCKS_PER_SEC) << "s";
	}

private:
	std::clock_t start;
};



void run(Result(*transfer)(vector<FixedBits*>&, FixedBits&), const int probabilityOfFixing, ostream& output)
{
	Stopwatch s;

	int unsatisfiableCount = 0;
	int satisfiableCount = 0;

	unsigned calls = 0;

	MTRand rand;

	unsigned long totalOutputBits = 0;
	unsigned long totalOutputOneBits = 0;
	unsigned long totalFixedOutputBits = 0;

	for (int i = 0; i < iterations; i++)
	{
		vector<FixedBits*> children;

		FixedBits a = FixedBits::createRandom(bitWidth, probabilityOfFixing, rand);
		FixedBits b = FixedBits::createRandom(bitWidth, probabilityOfFixing, rand);

		FixedBits output = FixedBits::createRandom(bitWidth, probabilityOfFixing, rand);

		for (unsigned i = 0; i < bitWidth; i++)
		{
			totalOutputBits++;
			if (output.isFixed(i))
			{
				totalFixedOutputBits++;
				if (output.getValue(i))
					totalOutputOneBits++;
			}
		}

		children.push_back(&a);
		children.push_back(&b);

		bool done = false;
		bool first = true;
		top: while (!done)
		{
			calls++;
			Result r = transfer(children, output);

			if (CONFLICT == r)
			{
				unsatisfiableCount++;
				done = true;
				if (!first)
					throw "BAD";
			}
			else
			{
				first = false;
				for (unsigned i = 0; i < bitWidth; i++)
				{
					if (!a.isFixed(i))
					{
						a.setFixed(i, true);
						a.setValue(i, false);
						goto top;
					}
					if (!b.isFixed(i))
					{
						b.setFixed(i, true);
						b.setValue(i, false);
						goto top;
					}
					if (!output.isFixed(i))
					{
						output.setFixed(i, true);
						output.setValue(i, false);
						goto top;
					}
				}
				satisfiableCount++;
				break;
			}
		}
	}

	output << "Iterations: " << iterations << endl;
	output << "BitWidth: " << bitWidth << endl;
	output << "Calls to transfer: " << calls << endl;
	output << "Probability of fixing: " << probabilityOfFixing << endl;
	output << "unsatisfiable count: " << unsatisfiableCount << endl;
	output << "satisfiable count: " << satisfiableCount << endl;
	output << "total: " << totalOutputBits << endl;
	output << "total fixed: " << totalFixedOutputBits << endl;
	output << "total One Bits fixed: " << totalOutputOneBits << endl;

	s.stop();

	return;
}

struct Relation
{
  FixedBits a,b,output;
  int initial;
  Relation(FixedBits a_, FixedBits b_, FixedBits output_)
  : a(a_), b(b_), output(output_)
  {
    initial = a.countFixed() + b.countFixed() + output.countFixed();
  }

};

void
runSimple(Result
(*transfer)(vector<FixedBits*>&, FixedBits&), const int probabilityOfFixing, ostream& output, Kind k)
{

  int conflicts = 0;

  MTRand rand;

  int initially_fixed = 0;
  int finally_fixed = 0;

  list<Relation> relations;

  for (int i = 0; i < iterations; i++)
    {
    FixedBits a = FixedBits::createRandom(bitWidth, 100, rand);
    FixedBits b = FixedBits::createRandom(bitWidth, 100, rand);

    assert(a.isTotallyFixed());
    assert(b.isTotallyFixed());
    ASTVec c;
    c.push_back(beev->CreateBVConst(a.GetBVConst(), bitWidth));
    c.push_back(beev->CreateBVConst(b.GetBVConst(), bitWidth));
    ASTNode result = NonMemberBVConstEvaluator(beev, k, c, bitWidth);
    FixedBits output = FixedBits::concreteToAbstract(result);

    for (int i = 0; i < a.getWidth(); i++)
      {
      if (rand.randInt() % 100 >= probabilityOfFixing)
        a.setFixed(i, false);
      if (rand.randInt() % 100 >= probabilityOfFixing)
        b.setFixed(i, false);
      }
    for (int i = 0; i < output.getWidth(); i++)
      if (rand.randInt() % 100 >= probabilityOfFixing)
        output.setFixed(i, false);

    Relation r(a, b, output);
    relations.push_back(r);
    }

  Stopwatch s;

  for (int i = 0; i < iterations; i++)
    {

    Relation& rel = relations.back();
    FixedBits& a = rel.a;
    FixedBits& b = rel.b;
    FixedBits& output = rel.output;

    vector<FixedBits*> children;
    children.push_back(&a);
    children.push_back(&b);

    Result r = transfer(children, output);

    assert(r != CONFLICT);

    const int final = a.countFixed() + b.countFixed() + output.countFixed();
    assert(final >= rel.initial);
    finally_fixed += final;
    initially_fixed += rel.initial;

    relations.pop_back();
    }

  clock_t t = s.stop2();

  cerr.setf(ios::fixed);
  cerr << "&" << setprecision(2) << (float(t) / CLOCKS_PER_SEC) << "s";
  output << " &" << initially_fixed << " & " << finally_fixed-initially_fixed << endl;

  return;
}



simplifier::constantBitP::Result signedModulus(vector<FixedBits*>& children,
                FixedBits& output)
{
        return bvSignedModulusBothWays(children, output,beev);
}

simplifier::constantBitP::Result signedRemainder(vector<FixedBits*>& children,
                FixedBits& output)
{
        return bvSignedRemainderBothWays(children, output,beev);
}


simplifier::constantBitP::Result unsignedDivision(vector<FixedBits*>& children,
                FixedBits& output)
{
        return bvUnsignedDivisionBothWays(children, output,beev);
}

simplifier::constantBitP::Result signedDivision(vector<FixedBits*>& children,
                FixedBits& output)
{
        return bvSignedDivisionBothWays(children, output,beev);
}


simplifier::constantBitP::Result multiply(vector<FixedBits*>& children,
                FixedBits& output)
{
        return bvMultiplyBothWays(children, output, beev);
}

//
void run_with_various_prob (Result(*transfer)(vector<FixedBits*>&, FixedBits&), ostream& output, Kind kind =UNDEFINED)
{
        int prob;

        runSimple(transfer, 1, cerr,kind);
        runSimple(transfer, 5, cerr,kind);
        runSimple(transfer, 50, cerr,kind);
        output << "\\\\" << endl;
}


int
main(void)
{
  beev = new BEEV::STPMgr();

  Cpp_interface interface(*beev, beev->defaultNodeFactory);
  interface.startup();

  srand(time(NULL));
  BEEV::ParserBM = beev;

  ostream& output = cerr;

  output << "signed greater than equals" << endl;
  run_with_various_prob(&bvSignedGreaterThanEqualsBothWays, output,BVSGE);

  output << "unsigned less than" << endl;
  run_with_various_prob(&bvLessThanBothWays, output,BVLT);

  output << "equals" << endl;
  run_with_various_prob(bvEqualsBothWays, output,EQ);

  output << "bit-vector xor" << endl;
  run_with_various_prob(bvXorBothWays, output,BVXOR);

  output << "bit-vector or" << endl;
  run_with_various_prob(bvOrBothWays, output,BVOR);

  output << "bit-vector and" << endl;
  run_with_various_prob(bvAndBothWays, output,BVAND);

  output << "right shift" << endl;
  run_with_various_prob(bvRightShiftBothWays, output, BVRIGHTSHIFT);

  output << "left shift" << endl;
  run_with_various_prob(bvLeftShiftBothWays, output, BVLEFTSHIFT);

  output << "arithmetic shift" << endl;
  run_with_various_prob(bvArithmeticRightShiftBothWays, output, BVSRSHIFT);

  output << "addition" << endl;
  run_with_various_prob(bvAddBothWays, output, BVPLUS);

  output << "multiplication" << endl;
  run_with_various_prob(multiply, output, BVMULT);

  output << "unsigned division" << endl;
  run_with_various_prob(unsignedDivision, output, BVDIV);

  output << "unsigned remainder" << endl;
  run_with_various_prob(signedRemainder, output, BVMOD);

  output << "signed division" << endl;
  run_with_various_prob(signedDivision, output, SBVDIV);

  output << "signed remainder" << endl;
  run_with_various_prob(signedRemainder, output, SBVREM);

  output << "%" << "iterations" << iterations;
  output << "%" << "bit-width" << bitWidth;
  return 1;
}


