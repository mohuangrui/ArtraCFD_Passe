/****************************************************************************
 * Space Domain Meshing                                                     *
 * Programmer: Huangrui Mo                                                  *
 * - Follow the Google's C/C++ style Guide.                                 *
 * - This file use ghost cell immersed boundary method to handle complex    *
 *   geometries                                                             *
 ****************************************************************************/
/****************************************************************************
 * Required Header Files
 ****************************************************************************/
#include "gcibm.h"
#include <stdio.h> /* standard library for input and output */
#include <math.h> /* common mathematical functions */
#include "commons.h"
/****************************************************************************
 * Static Function Declarations
 ****************************************************************************/
static int InitializeDomainGeometry(Space *, const Partition *);
static int LocateSolidGeometry(Space *, const Particle *, const Partition *);
static int IdentifyGhostNodes(Space *, const Partition *);
static int IdentifySolidNodeWithGhostNeighbours(Space *, const Particle *, const Partition *);
static int Min(const int x, const int y);
static int Max(const int x, const int y);
/****************************************************************************
 * Function definitions
 ****************************************************************************/
/*
 * These functions identify the type of each node: 
 * 1:                   boundary and exterior ghost node,
 * <= -offset:          interior solid node,
 * >=offset:            interior ghost node, 
 * 0:                   interior fluid node,
 * <= -offset - totalN: interior solid node with ghost neighbour.
 *
 * Procedures are:
 * -- initialize node flag of boundary and exterior nodes to boundary type,
 *    and inner nodes to fluid type;
 * -- identify all inner nodes that are in solid geometry as solid node;
 * -- identify ghost nodes according to the node type of its neighbours;
 * -- identify whether a solid node has ghost neighbours.
 *
 * It's necessary to difference boundary nodes and inner nodes because this
 * will make the identification of ghost nodes much easier in both 2D and
 * 3D situations.
 *
 * Moreover, whenever identifying a solid node or ghost node, store its
 * corresponding geometry information by linking the node flag to the
 * geometry ID, which will be accessed to calculate other informations. 
 * The rational is that don't store every information for each ghost node, but
 * only store necessary information. When need it, access and calculate it.
 */
int ComputeDomainGeometryGCIBM(Space *space, Particle *particle, const Partition *part)
{
    InitializeDomainGeometry(space, part);
    LocateSolidGeometry(space, particle, part);
    IdentifyGhostNodes(space, part);
    IdentifySolidNodeWithGhostNeighbours(space, particle, part);
    return 0;
}
static int InitializeDomainGeometry(Space *space, const Partition *part)
{
    /*
     * Set the vaule of offset to specify the range assignment for node type
     * identifier.
     */
    space->nodeFlagOffset = 10;
    /*
     * Initialize the entire domain to boundary type. Operation can be achieved
     * by a single loop since all data are stored by linear arrays.
     */
    int idx = 0; /* linear array index math variable */
    for (idx = 0; idx < space->nMax; ++idx) {
        space->nodeFlag[idx] = 1;
    }
    /*
     * Initialize inner nodes to fluid type.
     */
    for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
        for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
            for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                idx = (k * space->jMax + j) * space->iMax + i;
                space->nodeFlag[idx] = 0;
            }
        }
    }
    return 0;
}
/*
 * When locate solid nodes, there are two approaches available. One is search
 * over each node and verify each node regarding to all the particles; another
 * is search each particle and find all the nodes inside current particle.
 * The second method is adopted here for performance reason, although it's much
 * more complicated than the first one.
 */
static int LocateSolidGeometry(Space *space, const Particle *particle, const Partition *part)
{
    int idx = 0; /* linear array index math variable */
    /* geometry computation */
    Real distance = 0.0;
    Real distX = 0.0;
    Real distY = 0.0;
    Real distZ = 0.0;
    Real radius = 0.0;
    const Real *ptk = NULL;
    const int offset = space->nodeFlagOffset;
    for (int geoCount = 0; geoCount < particle->totalN; ++geoCount) {
        ptk = particle->headAddress + geoCount * particle->entryN; /* point to storage of current particle */
        const int iCenter = (int)((ptk[0] - space->xMin) * space->ddx) + space->ng;
        const int jCenter = (int)((ptk[1] - space->yMin) * space->ddy) + space->ng;
        const int kCenter = (int)((ptk[2] - space->zMin) * space->ddz) + space->ng;
        const Real safetyCoe = 1.5; /* zoom the search range */
        const int iRange = (int)(safetyCoe * ptk[3] * space->ddx);
        const int jRange = (int)(safetyCoe * ptk[3] * space->ddy);
        const int kRange = (int)(safetyCoe * ptk[3] * space->ddz);
        const int kSub = Max(kCenter - kRange, part->kSub[0]);
        const int kSup = Min(kCenter + kRange + 1, part->kSup[0]);
        const int jSub = Max(jCenter - jRange, part->jSub[0]);
        const int jSup = Min(jCenter + jRange + 1, part->jSup[0]);
        const int iSub = Max(iCenter - iRange, part->iSub[0]);
        const int iSup = Min(iCenter + iRange + 1, part->iSup[0]);
        for (int k = kSub; k < kSup; ++k) {
            for (int j = jSub; j < jSup; ++j) {
                for (int i = iSub; i < iSup; ++i) {
                    idx = (k * space->jMax + j) * space->iMax + i;
                    distX = space->xMin + (i - space->ng) * space->dx - ptk[0];
                    distY = space->yMin + (j - space->ng) * space->dy - ptk[1];
                    distZ = space->zMin + (k - space->ng) * space->dz - ptk[2];
                    distance = distX * distX + distY * distY + distZ * distZ - ptk[3] * ptk[3];
                    if (0 > distance) { /* in the solid geometry */
                        space->nodeFlag[idx] = -offset - geoCount; /* geometry are linked */
                    }
                }
            }
        }
    }
    return 0;
}
static int IdentifyGhostNodes(Space *space, const Partition *part)
{
    int idx = 0; /* linear array index math variable */
    int idxW = 0; /* index at West */
    int idxE = 0; /* index at East */
    int idxS = 0; /* index at South */
    int idxN = 0; /* index at North */
    int idxF = 0; /* index at Front */
    int idxB = 0; /* index at Back */
    /* criteria */
    int flag = 0;
    const int offset = space->nodeFlagOffset;
    for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
        for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
            for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                idx = (k * space->jMax + j) * space->iMax + i;
                if (-offset < space->nodeFlag[idx]) { /* it's not solid node */
                    continue;
                }
                idxW = (k * space->jMax + j) * space->iMax + i - 1;
                idxE = (k * space->jMax + j) * space->iMax + i + 1;
                idxS = (k * space->jMax + j - 1) * space->iMax + i;
                idxN = (k * space->jMax + j + 1) * space->iMax + i;
                idxF = ((k - 1) * space->jMax + j) * space->iMax + i;
                idxB = ((k + 1) * space->jMax + j) * space->iMax + i;
                flag = space->nodeFlag[idxW] * space->nodeFlag[idxE] * 
                    space->nodeFlag[idxS] * space->nodeFlag[idxN] * 
                    space->nodeFlag[idxF] * space->nodeFlag[idxB];
                if (0 == flag) { /* if exist one neighbour is fluid, then it's ghost */
                    space->nodeFlag[idx] = -space->nodeFlag[idx]; /* geometry information conserved */
                }
            }
        }
    }
    return 0;
}
static int IdentifySolidNodeWithGhostNeighbours(Space *space, const Particle *particle, const Partition *part)
{
    int idx = 0; /* linear array index math variable */
    int idxW = 0; /* index at West */
    int idxE = 0; /* index at East */
    int idxS = 0; /* index at South */
    int idxN = 0; /* index at North */
    int idxF = 0; /* index at Front */
    int idxB = 0; /* index at Back */
    const int offset = space->nodeFlagOffset;
    for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
        for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
            for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                idx = (k * space->jMax + j) * space->iMax + i;
                if (-offset < space->nodeFlag[idx]) { /* it's not solid node */
                    continue;
                }
                idxW = (k * space->jMax + j) * space->iMax + i - 1;
                idxE = (k * space->jMax + j) * space->iMax + i + 1;
                idxS = (k * space->jMax + j - 1) * space->iMax + i;
                idxN = (k * space->jMax + j + 1) * space->iMax + i;
                idxF = ((k - 1) * space->jMax + j) * space->iMax + i;
                idxB = ((k + 1) * space->jMax + j) * space->iMax + i;
                if ((-offset >= space->nodeFlag[idxW]) && (-offset >= space->nodeFlag[idxE]) &&  
                        (-offset >= space->nodeFlag[idxS]) && (-offset >= space->nodeFlag[idxN]) && 
                        (-offset >= space->nodeFlag[idxF]) && (-offset >= space->nodeFlag[idxB])) {
                    continue; /* this solid node has no ghost neighbour */
                }
                /* exist at least one neighbour is ghost */
                space->nodeFlag[idx] = space->nodeFlag[idx] - particle->totalN; /* geometry information conserved */
            }
        }
    }
    return 0;
}
/*
 * Boundary condition for interior ghost nodes and solid nodes with ghost
 * neighbours.
 */
int BoundaryConditionGCIBM(Real *U, const Space *space, const Particle *particle, 
        const Partition *part, const Flow *flow)
{
    int idx = 0; /* linear array index math variable */
    int geoID = 0; /* geometry id */
    const int offset = space->nodeFlagOffset;
    /*
     * Processing ghost nodes
     */
    for (int k = part->kSub[0]; k < part->kSup[0]; ++k) {
        for (int j = part->jSub[0]; j < part->jSup[0]; ++j) {
            for (int i = part->iSub[0]; i < part->iSup[0]; ++i) {
                idx = (k * space->jMax + j) * space->iMax + i;
                if (offset > space->nodeFlag[idx]) { /* it's not a ghost */
                    continue;
                }
                geoID = space->nodeFlag[idx] - offset; /* extract geometry number from inner ghost node flag */
            }
        }
    }
    return 0;
}
/*
 * Linear reconstruction is adopted here, variable phi at ghost or solid node
 * is reconstructed by two steps:
 *
 * phi = 2 * phi_o - phi_image 
 * (scalar phi = phi_o = phi_image, vector phi_o = 0, thus phi = - phi_image)
 * phi_image = a0 + a1 * x + a2 * y + a3 * z
 *
 * ai are undetermined coefficients which will be determined by 
 * solving a linear system at neighbour nodes of the image point under the
 * assumption that linear distribution of phi(x,y,z) is also valid for the
 * neighbour nodes.
 */
static int LinearReconstruction(const int k, const int j, const int i, const int geoID, 
        Real *U, const Space *space, const Particle *particle, const Flow *flow)
{
    Real coeVector[4] = {0.0}; /* undetermined coefficients vector */
    Real posMatrix[4][4] = {{0.0}}; /* position matrix for interpolation */
    Real rhsVector[4][5] = {{0.0}}; /* right hand side vectors for five variables */
    const Real *ptk = particle->headAddress + geoID * particle->entryN; /* point to storage of current particle */
    const Real distX = space->xMin + (i - space->ng) * space->dx - ptk[0];
    const Real distY = space->yMin + (j - space->ng) * space->dy - ptk[1];
    const Real distZ = space->zMin + (k - space->ng) * space->dz - ptk[2];
    const Real distToCenter = sqrt(distX * distX + distY * distY + distZ * distZ);
    const Real normalX = distX / distToCenter;
    const Real normalY = distY / distToCenter;
    const Real normalZ = distZ / distToCenter;
    const Real distToSurface = ptk[3] - distToCenter;
    /* obtain the coordinates of the image point */
    const Real imageX = space->xMin + (i - space->ng) * space->dx + 2 * distToSurface * normalX;
    const Real imageY = space->yMin + (j - space->ng) * space->dy + 2 * distToSurface * normalY;
    const Real imageZ = space->zMin + (k - space->ng) * space->dz + 2 * distToSurface * normalZ;
    const int imageI = i + (int)(2 * distToSurface * normalX * space->ddx);
    const int imageJ = j + (int)(2 * distToSurface * normalY * space->ddy);
    const int imageK = k + (int)(2 * distToSurface * normalZ * space->ddz);
    /*
     * Originally the interpolation stencil should contain the boundary point,
     * however, this will complicate the problem very much. Therefore, the
     * influence of boundary conditions at wall will only be applied in the
     * first relationship, and the interpolation stencils are all fluid node.
     */
    /* 
     * Search around the image node to find required fluid nodes as
     * interpolation stencil. Because the node coordinates of the image node
     * are always downward truncated, therefore, the prior search directions 
     * are better to set as 0 (current node) or +1 (upward direction).
     */
    /* build an adequate search path */
    const int path[27][3] = { /* n paths for i, j, k */
        {0, 0, 0}, {1, 1, 1}, {1, 1, 0}, {1, 0, 1},
        {0, 1, 1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
        {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, {-1, 1, 0},
        {-1, 0, 1}, {1, -1, 0}, {0, -1, 1}, {1, 0, -1},
        {0, 1, -1}, {-1, 1, 1}, {1, -1, 1}, {1, 1, -1},
        {-1, -1, 0}, {-1, 0, -1}, {0, -1, -1}, {-1, -1, 1},
        {-1, 1, -1}, {1, -1, -1}, {-1, -1, -1}};
    const int stencilN = 4; /* number of stencils for interpolation */
    int tally = 0; /* number of current stencil */
    for (int loop = 0; (tally < stencilN) && (loop < 27); ++loop) {
        const int ih = imageI + path[loop][0];
        const int jh = imageJ + path[loop][1];
        const int kh = imageK + path[loop][2];
        const int idxh = (kh * space->jMax + jh) * space->iMax + ih;
        if (0 != space->nodeFlag[idxh]) { /* it's not a fluid node */
            continue;
        }
        /* 
         * Obtain the coordinates of the stencil and save to matrix. One
         * approach is to save the space coordinates to construct the matrix
         * for the linear system. However, this will easily result a singular
         * matrix or a matrix which can not be easily processed by Gaussian
         * elimination or LU decomposition even with pivoting. The second
         * approach is to use the node coordinates to do the construction,
         * which is equivalent because of the same degree of freedom. 
         */
        posMatrix[tally][0] = 1;
        posMatrix[tally][1] = (Real)(ih); 
        posMatrix[tally][2] = (Real)(jh);
        posMatrix[tally][3] = (Real)(kh);
        /* construct the right hand vectors */
        const Real rho_h = U[idxh+0];
        const Real u_h = U[idxh+1] / rho_h;
        const Real v_h = U[idxh+2] / rho_h;
        const Real w_h = U[idxh+3] / rho_h;
        const Real eT_h = U[idxh+4] / rho_h;
        const Real p_h = (flow->gamma - 1.0) * rho_h * (eT_h - 0.5 * (u_h * u_h + v_h * v_h + w_h * w_h));
        rhsVector[tally][0] = rho_h;
        rhsVector[tally][1] = u_h;
        rhsVector[tally][2] = v_h;
        rhsVector[tally][3] = w_h;
        rhsVector[tally][4] = p_h;
        ++tally; /* increase the tally */
    }
}
/*
 * Perform A = LU factorization, A is a n x n matrix.
 * After factorization, both L and U are stored into the storage space of A,
 * The returned A has its upper triangle as U, and the part beneath the 
 * diagonal is equal to L. The missing diagonal elements of L are all 1.
 * Crout's algorithm is employed for factorization.
 *
 * Partial pivoting is used to stabilize the algorithm, that is, only row-wise
 * permutation will be employed. The permutations are recorded in the integer 
 * vector "permute", which will be used to reorder the right hand side vectors
 * before solving the linear system.
 */
static int LUFactorization(Real **A, const int n, int permute[])
{
    const Real epsilon = 1.0e-15; /* a small number for singularity check */
    Real temp = 0.0; /* auxiliary variable */
    Real scale[n]; /* stores the implicit scaling of each row with variable length array */
    int rowMax = 0; /* record the row number of current pivot element */
    int flag = 1; /* changes sign with each row interchange */
    /*
     * Loop over rows to get the implicit scaling information.
     */
    for (int row = 0; row < n; ++row) {
        Real maximum = 0.0;
        for (int col = 0; col < n; ++col) {
            temp = fabs(A[row][col]);
            if (temp > maximum) {
                maximum = temp;
            }
        }
        if (0 == maximum) {
            FatalError("singular matrix in LU factorization...");
        }
        scale[row] = 1.0 / maximum; /* save the scaling */
    }
    /*
     * Do LU factorization with partial pivoting.
     */
    for (int loop = 0; loop < n; ++loop) {
        Real maximum = 0.0; /* initialize for the search of largest pivot element */
        rowMax = loop; /* initialize the pivot position to current row */
        for (int row = loop; row < n; ++row) { /* search pivot element for current loop */
            temp = scale[row] * fabs(A[row][loop]);
            if (temp > maximum) {
                maximum = temp;
                rowMax = row;
            }
        }
        if (loop != rowMax) { /* interchange rows if required */
            for (int col = 0; col < n; ++col) {
                temp = A[rowMax][col];
                A[rowMax][col] = A[loop][col];
                A[loop][col] = temp;
            }
            flag = -flag; /* change the parity of flag */
            scale[rowMax] = scale[loop]; /* replace the scale factor */
        }
        permute[loop] = rowMax; /* record the permutation */
        if (0 == A[loop][loop]) { /* substitute zero pivot element with epsilon */
            A[loop][loop] = epsilon;
        }
        for (int row = loop + 1; row < n; ++row) {
            A[row][loop] = A[row][loop] / A[loop][loop]; /* divide column element with pivot element */
            for (int col = loop + 1; col < n; ++col) {
                A[row][col] = A[row][col] - A[row][loop] * A[loop][col];
            }
        }
    }

}
static int Min(const int x, const int y)
{
    if (x < y) {
        return x;
    }
    return y;
}
static int Max(const int x, const int y)
{
    if (x > y) {
        return x;
    }
    return y;
}
/* a good practice: end file with a newline */

