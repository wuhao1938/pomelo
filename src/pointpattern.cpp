#include <iostream>
#include "pointpattern.hpp"


void pointpattern::addpoint(double x, double y, double z, int l)
{
    point p (x,y,z,l);
    points.push_back(p);
}

void pointpattern::print ()
{
    std::cout << "number of points " << points.size() << std::endl;

}

void pointpattern::removeduplicates (double epsilon)
{
    if(points.empty()) return;
    std::vector<point> newpoints;
    for(unsigned int i = 0; i != points.size(); ++i)
    {
        if(i%1000==0)
        {
            std::cout << i << " / " <<  points.size() << "\n";
        }
        bool addthis =true;
        point p1 = points[i];
        for(unsigned int j = i+1; j != points.size(); ++j)
        {
            if (j >= points.size()) break;
            point p2 = points[j];
            if (checkdistancecloserthan(p1, p2, epsilon))
            {
                //std::cout << " point "<<   i << " and point " << j << " too close together" << "\n";
                addthis = false;
                break;
            }
        }
        if (addthis)
        {
            //std::cout << "adding" << std::endl;
            newpoints.push_back(p1);
        }
        else
        {
            //std::cout << "removing point" << std::endl;
        }
    }
    points = newpoints;
}

void pointpattern::removeduplicates (double epsilon, pointpattern& p)
{
    if(points.empty()) return;
    std::vector<point> newpoints;
    for(unsigned int i = 0; i != points.size(); ++i)
    {
        if(i%1000==0)
        {
            std::cout << i << " / " <<  points.size() << "\n";
        }
        bool addthis =true;
        point p1 = points[i];
        for(unsigned int j = 0; j != p.points.size(); ++j)
        {
            point p2 = p.points[j];
            if (checkdistancecloserthan(p1, p2, epsilon))
            {
                //std::cout << " point "<<   i << " and point " << j << " too close together" << "\n";
                addthis = false;
                break;
            }
        }
        if (addthis)
        {
            //std::cout << "adding" << std::endl;
            newpoints.push_back(p1);
        }
        else
        {
            //std::cout << "removing point" << std::endl;
        }
    }
    points = newpoints;
}
