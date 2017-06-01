#ifndef POISSONGRID_H
#define POISSONGRID_H

/**
 * \file PoissonGenerator.h
 * \brief
 *
 * Poisson Disk Points Generator
 *
 * \version 1.1.4
 * \date 19/10/2016
 * \author Sergey Kosarevsky, 2014-2016
 * \author support@linderdaum.com   http://www.linderdaum.com   http://blog.linderdaum.com
 */

/*
    Usage example:

        #define POISSON_PROGRESS_INDICATOR 1
        #include "PoissonGenerator.h"
        ...
        PoissonGenerator::DefaultPRNG PRNG;
        const auto Points = PoissonGenerator::GeneratePoissonPoints( NumPoints, PRNG );
*/

// Fast Poisson Disk Sampling in Arbitrary Dimensions
// http://people.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf

// Implementation based on http://devmag.org.za/2009/05/03/poisson-disk-sampling/

/* Versions history:
 *		1.1.4 	Oct 19, 2016		POISSON_PROGRESS_INDICATOR can be defined outside of the header file, disabled by default
 *		1.1.3a	Jun  9, 2016		Update constructor for DefaultPRNG
 *		1.1.3		Mar 10, 2016		Header-only library, no global mutable state
 *		1.1.2		Apr  9, 2015		Output a text file with XY coordinates
 *		1.1.1		May 23, 2014		Initialize PRNG seed, fixed uninitialized fields
 *    1.1		May  7, 2014		Support of density maps
 *		1.0		May  6, 2014
*/

#include <vector>
#include <random>
#include <stdint.h>
#include <time.h>

namespace PoissonGenerator {

const char* Version = "1.1.4 (19/10/2016)";

class DefaultPRNG {
public:
    DefaultPRNG()
        : m_Gen(std::random_device()()),
          m_Dis(0.0f, 1.0f)
    {
        // prepare PRNG
        m_Gen.seed(ulong(time(nullptr)));
    }

    explicit DefaultPRNG(uint32_t seed)
        : m_Gen(seed),
          m_Dis(0.0f, 1.0f)
    {
    }

    inline float RandomFloat() {
        return static_cast<float>(m_Dis(m_Gen));
    }

    inline int RandomInt(int Max) {
        std::uniform_int_distribution<> DisInt(0, Max);
        return DisInt(m_Gen);
    }

private:
    std::mt19937 m_Gen;
    std::uniform_real_distribution<float> m_Dis;
};

struct sPoint {
    constexpr sPoint() : x(0), y(0), m_Valid(false) {}
    constexpr sPoint(float X, float Y) : x(X), y(Y), m_Valid(true) {}

    //
    inline bool IsInRectangle() const {
        return x >= 0 && y >= 0 && x <= 1 && y <= 1;
    }
    //
    inline bool IsInCircle() const {
        float fx = x - 0.5f;
        float fy = y - 0.5f;
        return ( fx*fx + fy*fy ) <= 0.25f;
    }

    inline float Dist2(sPoint &P) {
        return (x - P.x) * (x - P.x) + (y - P.y) * (y - P.y);
    }

    float x;
    float y;
    bool m_Valid;
};

struct sGridPoint {
    int x;
    int y;
};

struct sGrid {
    sGrid( int W, int H)
        : m_W(W)
        , m_H(H)
    {
        m_Grid.resize(m_H);
        for (auto &r : m_Grid)
            r.resize(m_W);
    }

    inline sGridPoint GridPoint(const sPoint &P) const {
        return { int(P.x * m_W), int(P.y * m_H) };
    }

    inline void Insert(const sPoint &P) {
        auto G = GridPoint(P);
        m_Grid[G.x][G.y] = P;
    }

    bool IsInNeighbourhood(sPoint Point, float MinDist2) {
        sGridPoint G = GridPoint(Point);

        // number of adjucent cells to look for neighbour points
        const int D = 5;

        // scan the neighbourhood of the point in the grid
        for ( int i = G.x - D; i < G.x + D; i++ )
        {
            for ( int j = G.y - D; j < G.y + D; j++ )
            {
                if ( i >= 0 && i < m_W && j >= 0 && j < m_H )
                {
                    sPoint P = m_Grid[i][j];

                    if ( P.m_Valid && P.Dist2(Point) < MinDist2 ) { return true; }
                }
            }
        }

        return false;
    }

private:
    int m_W;
    int m_H;
    std::vector<std::vector<sPoint>> m_Grid;
};

template <typename PRNG>
sPoint PopRandom(std::vector<sPoint> &Points, PRNG &Generator)
{
    const int Idx = Generator.RandomInt(Points.size()-1);
    auto P = Points[Idx];
    std::iter_swap(Points.begin() + Idx, Points.end()-1);
    Points.pop_back();
    return P;
}

template <typename PRNG>
sPoint GenerateRandomPointAround(const sPoint& P, float MinDist, PRNG& Generator)
{
    // start with non-uniform distribution
    float R1 = Generator.RandomFloat();

    // radius should be between MinDist and 2 * MinDist
    float Radius = MinDist * ( R1 + 1.0f );

    // random angle
    float C = 2.0f * Generator.RandomFloat() - 1.0f; // cos
    float S = 2.0f * Generator.RandomFloat() - 1.0f; // sin

    // the new point is generated around the point (x, y)
    float X = P.x + Radius * C;
    float Y = P.y + Radius * S;

    return sPoint( X, Y );
}

/**
    Return a vector of generated points

    NewPointsCount - refer to bridson-siggraph07-poissondisk.pdf for details (the value 'k')
    Circle  - 'true' to fill a circle, 'false' to fill a rectangle
    MinDist - minimal distance estimator, use negative value for default
**/
template <typename PRNG = DefaultPRNG>
std::vector<sPoint> GeneratePoissonPoints(
    size_t NumPoints,
    PRNG& Generator,
    int NewPointsCount = 30,
    bool Circle = true,
    float MinDist = -1.0f
)
{
    if (MinDist < 0.0f)
        MinDist = sqrt(float(NumPoints)) / float(NumPoints);

    std::vector<sPoint> SamplePoints;
    std::vector<sPoint> ProcessList;

    // create the grid
    float CellSize = MinDist / sqrt(2.0f);
    float MinDist2 = MinDist * MinDist;

    int GridW = (int)ceil(1.0f / CellSize);
    int GridH = (int)ceil(1.0f / CellSize);

    sGrid Grid(GridW, GridH);
    sPoint FirstPoint;

    do {
        FirstPoint = sPoint( Generator.RandomFloat(), Generator.RandomFloat() );
    } while (!(Circle ? FirstPoint.IsInCircle() : FirstPoint.IsInRectangle()));

    // update containers
    ProcessList.push_back( FirstPoint );
    SamplePoints.push_back( FirstPoint );
    Grid.Insert( FirstPoint );

    // generate new points for each point in the queue
    while ( !ProcessList.empty() && SamplePoints.size() < NumPoints )
    {
        sPoint Point = PopRandom<PRNG>( ProcessList, Generator );

        for ( int i = 0; i < NewPointsCount; i++ )
        {
            sPoint NewPoint = GenerateRandomPointAround( Point, MinDist, Generator );

            bool Fits = Circle ? NewPoint.IsInCircle() : NewPoint.IsInRectangle();

            if ( Fits && !Grid.IsInNeighbourhood( NewPoint, MinDist2 ) )
            {
                ProcessList.push_back( NewPoint );
                SamplePoints.push_back( NewPoint );
                Grid.Insert( NewPoint );
            }
        }
    }

    return SamplePoints;
}

} // namespace PoissonGenerator


#endif // POISSONGRID_H
