#include <iostream>
#include <fstream>
#include <map>
#include <limits>
#include <sys/stat.h>
#include "include.hpp"
#include "fileloader.hpp"
#include "pointpattern.hpp"
#include "duplicationremover.hpp"
#include "polywriter.hpp"
using namespace sel;
using namespace voro;


void  getFaceVerticesOfFace( std::vector<int>& f, unsigned int k, std::vector<unsigned int>& vertexList)
{
    vertexList.clear();

    // iterate through face-vertices vector bracketed
    // (number of vertices for face 1) vertex 1 ID face 1, vertex 2 ID face 1 , ... (number of vertices for face 2, ...
    unsigned long long index = 0;
    // we are at face k, so we have to iterate through the face vertices vector up to k
    for (unsigned long long cc = 0; cc <= k; cc++)
    {

        unsigned long long b = f[index];    // how many vertices does the current face (index) have?
        // iterate index "number of vertices of this face" forward
        for (unsigned long long bb = 1; bb <= b; bb++)
        {
            index++;
            // if we have found the correct face, get all the vertices for this face and save them
            if (cc == k)
            {
                //std::cout << "\t" << f[index] << std::endl;
                int vertexindex = f[index];
                vertexList.push_back(vertexindex);
            }
        }
        index++;
    }
}


int main (int argc, char* argv[])
{

    // command line argument parsing
    if(argc != 3 )
    {
        std::cerr << "Commandline parameters not correct .... aborting "  << std::endl;
        std::cerr << std::endl <<  "Use setvoronoi this way:\n\t ./setvoronoi [path-to-lua-file] [outputfolder]"  << std::endl;
        return -1;
    }

    const std::string filename = argv[1];
    std::string folder = argv[2];

    // lua state for the global parameter file
    State state {true};
    state.Load(filename);

    // command line sanity check
    if(folder.empty())
    {
        throw std::string("outfilepath is empty");
    }

    // sanitize folder path and create folder 
    char lastCharOfFolder = *folder.rbegin();
    if (lastCharOfFolder != '/')
        folder += '/';
    mkdir(folder.c_str(),0755);


    // parse global parameters from lua file
    std::string posfile = state["positionfile"];
    std::string readfile = state["readfile"];

    std::cout << "Working on " << posfile << " " << readfile << std::endl;

    // read particle parameters and positions
    std::vector<particleparameterset> setlist;
    fileloader loader;
    loader.read(posfile,setlist);

    // pp contains the triangulation of the particle surfaces
    pointpattern pp;
    
    // scope for the readstate to ensure it won't lack out to anything else
    {
        // create a readstate that translates the particle parameters to surface shapes
        State readstate {true};
        readstate["pointpattern"].SetClass<pointpattern> ("addpoint", &pointpattern::addpoint );
        readstate.Load(readfile);


        for(auto it = setlist.begin(); it != setlist.end(); ++it )
        {
            // read one particle from the position file
            particleparameterset set = (*it);
            // put all parameters for this particle to the lua readstate
            for(unsigned int i = 0; i != set.parameter.size(); ++i)
            {
                readstate["s"][i] = set.get(i);
            }

            // let the lua readstate calculate the surface triangulation for this particle
            readstate["docalculation"](pp);
        }
    }

    // parse epsilon from the global lua parameter file
    const double epsilon = state["epsilon"];
    // parse boundaries from the global lua parameter file
    const double xmin = state["xmin"];
    const double ymin = state["ymin"];
    const double zmin = state["zmin"];
    const double xmax = state["xmax"];
    const double ymax = state["ymax"];
    const double zmax = state["zmax"];
    bool xpbc = false;
    bool ypbc = false;
    bool zpbc = false;

    std::string boundary = state["boundary"];
    if(boundary == "periodic")
    {
        xpbc = state["xpbc"];
        ypbc = state["ypbc"];
        zpbc = state["zpbc"];
    }
    else if (boundary == "none")
    {
        std::cout << "boundary condition mode 'none' selected." << std::endl;
    }
    else
    {
        std::cerr << "bondary condition mode " << boundary << " not known" << std::endl;
    }

    // clean degenerated vertices from particle surface triangulation pointpattern
    std::cout << "remove duplicates" << std::endl;
    duplicationremover d(16,16,16);
    d.setboundaries(xmin, xmax, ymin, ymax, zmin, zmax);
    d.addPoints(pp);
    d.removeduplicates(epsilon);
    d.getallPoints(pp);

    // print out pointpattern file
    {
        std::cout << "save point pattern file" << std::endl;
        std::ofstream file;
        file.open(folder + "surface_triangulation.xyz");
        file << pp;
        file.close();
    }


    // add particle surface triangulation to voro++ pre container for subcell division estimate
    std::cout << "importing surface triangulation to voro++" << std::endl;

    pre_container pcon(xmin, xmax, ymin, ymax, zmin, zmax, xpbc, ypbc, zpbc);
    
    // label ID map is used to  map surface point IDs to the respective particle label
    std::cout << "creating label id map " ;
    std::map < unsigned long long, unsigned long long > labelidmap;

    std::vector<std::vector<double> > ref;
    {
        unsigned long long id = 0;
        for(    auto it = pp.points.begin();
                it != pp.points.end();
                ++it)
        {
            pcon.put(id, it->x, it->y, it->z);
            unsigned int l = it->l;
            labelidmap[id] = l;
            ++id;
            if(ref.size() < l+1) ref.resize(l+1);
            if(ref[l].size() == 0)
            {
                ref[l].push_back(it->x);
                ref[l].push_back(it->y);
                ref[l].push_back(it->z);
                ref[l].push_back(0);
                ref[l].push_back(0);
                ref[l].push_back(0);
            }
        }
    }
    unsigned long long numberofpoints = labelidmap.size();
    std::cout << "finished" << std::endl;
    
    // setting up voro++ container
    int nx, ny, nz;
    pcon.guess_optimal(nx,ny,nz);
    container con(xmin, xmax, ymin, ymax, zmin, zmax, nx, ny, nz, xpbc, ypbc, zpbc, 8);
    pcon.setup(con);
    std::cout << "setting up voro++ container with division: (" << nx << " " << ny << " " << nz << ") for N= " << numberofpoints << " particles " << std::endl;

    // postprocessing for normal voronoi Output
    {
        std::string customFileName = folder + "custom.stat";
        con.print_custom("%i %s %v", customFileName);
    }

    // merge voronoi cells to set voronoi diagram
    std::cout << "merge voronoi cells ";
    
    pointpattern ppreduced;
    polywriter pw;
    // loop over all voronoi cells
    c_loop_all cla(con);
    // cell currently worked on
    unsigned long long status = 0;
    // counter for process output
    double tenpercentSteps = 0.1*static_cast<double>(numberofpoints);
    double target = tenpercentSteps;

    double xdist = xmax - xmin;
    double ydist = ymax - ymin;
    double zdist = zmax - zmin;

    //unsigned long long refl = 0;

    if(cla.start())
    {
        std::cout << "started\n" << std::flush;
        do
        {
            voronoicell_neighbor c;
            status++;
            if(con.compute_cell(c,cla))
            {
                if ( status >= target)
                {
                    target += tenpercentSteps;
                    std::cout << static_cast<int>(static_cast<double>(status)/static_cast<double>(numberofpoints)*100) << " \%\t" << std::flush;
                }
                //std::cout << "computed"  << std::endl;
                double xc = 0;
                double yc = 0;
                double zc = 0;
                // Get the position of the current particle under consideration
                cla.pos(xc,yc,zc);

                unsigned int id = cla.pid();
                unsigned long long l = labelidmap[id];

                double xabs = (xc-ref[l][0]);
                double yabs = (yc-ref[l][1]);
                double zabs = (zc-ref[l][2]);

                double xabs2 = xabs*xabs;
                double yabs2 = yabs*yabs;
                double zabs2 = zabs*zabs;

                ref[l][3]=0;
                if(xabs2 > 4)
                {
                    if(xabs < 0) ref[l][3] = xdist;
                    else ref[l][3] = -xdist;
                }
                ref[l][4]=0;
                if(yabs2 > 4)
                {
                    if(yabs < 0) ref[l][4] = ydist;
                    else ref[l][4] = -ydist;
                }
                ref[l][5]=0;
                if(zabs2 > 4)
                {
                    if(zabs < 0) ref[l][5] = zdist;
                    else ref[l][5] = -zdist;
                }

                std::vector<int> f; // list of face vertices (bracketed, as ID)
                c.face_vertices(f);

                std::vector<double> vertices;   // all vertices for this cell
                c.vertices(xc,yc,zc, vertices);

                std::vector<int> w; // neighbors of faces
                c.neighbors(w);
                // for this cell, loop over all faces and get the corresponding neighbors
                for (unsigned long long k = 0; k != w.size(); ++k)
                {
                    // compare if id for this cell and the face-neighbor is the same
                    int n = w[k];   // ID of neighbor cell
                    if (labelidmap[n] == l)
                    {
                        // discard this neighbour/face, since they have the same id
                        // std::cout << "discarding face " << l << " " << labelidmap[n] << std::endl;
                    }
                    else
                    {
                        std::vector<unsigned int> facevertexlist;
                        getFaceVerticesOfFace(f, k, facevertexlist);
                        std::vector<double> positionlist;
                        for (
                            auto it = facevertexlist.begin();
                            it != facevertexlist.end();
                            ++it)
                        {
                            unsigned int vertexindex = (*it);
                            double x = vertices[vertexindex*3];
                            double y = vertices[vertexindex*3+1];
                            double z = vertices[vertexindex*3+2];

                            if(xpbc) x += ref[l][3];
                            if(ypbc) y += ref[l][4];
                            if(zpbc) z += ref[l][5];

                            ppreduced.addpoint(l,x,y,z);
                            positionlist.push_back(x);
                            positionlist.push_back(y);
                            positionlist.push_back(z);
                        }
                        pw.addface(positionlist, l);

                    }
                }

            }
        }
        while (cla.inc());
    }
    std::cout << std::endl << " finished with N= " << ppreduced.points.size() << std::endl;

    // save point pattern output
    pw.savePointPatternForGnuplot(folder + "reduced.xyz");

    // remove duplicates and label back indices
    pw.removeduplicates(epsilon, xmin, xmax, ymin, ymax, zmin, zmax);

    // Write poly file for karambola
    {
        std::cout << "writing poly file" << std::endl;
        std::ofstream file;
        file.open(folder + "cell.poly");
        file << pw;
        file.close();
    }
    std::cout << "working for you has been nice. Thank you for using me & see you soon. :) "<< std::endl;

    return 0;
}
