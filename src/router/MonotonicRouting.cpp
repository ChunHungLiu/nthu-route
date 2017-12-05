/*
 * MonotonicRouting.cpp
 *
 *  Created on: Nov 29, 2017
 *      Author: florian
 */

#include "MonotonicRouting.h"

#include <bits/move.h>
#include <boost/multi_array/base.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../grdb/EdgePlane.h"
#include "Congestion.h"
#include "DataDef.h"

MonotonicRouting::MonotonicRouting(Congestion& congestion, bool enable) :
        congestion { congestion }, //
        cong_monotonic { boost::extents[congestion.congestionMap2d.getXSize()][congestion.congestionMap2d.getYSize()] }, //
        parent_monotonic { boost::extents[congestion.congestionMap2d.getXSize()][congestion.congestionMap2d.getYSize()] }, //
        monotonic_enable { enable }
//
{

}

MonotonicRouting::~MonotonicRouting() {
    // TODO Auto-generated destructor stub
}
void MonotonicRouting::monotonic_routing_algorithm(int x1, int y1, int x2, int y2, OrientationType dir, int net_id, Bound& bound) {
    int i, j;
    double cost;
    int distance = 1;

    Monotonic_element& ele1 = cong_monotonic[x1][y1];

//initialize cong_monotonic and parent_monotonic
    ele1.max_cost = -1000000;
    ele1.total_cost = 0;
    ele1.distance = 0;
    ele1.net_cost = 0;
    ele1.via_num = 0;
    parent_monotonic[x1][y1] = -1;
//Update the cost of top boundary or bottom boundary, which draw with double line.
//The source can in left-top corner or left-bottom corner
    for (i = x1 + 1; i <= x2; ++i) {
        if (parent_monotonic[i - 1][y1] != -2) {
            cost = congestion.get_cost_2d(i, y1, LEFT, net_id, &distance);
            Monotonic_element& ele = cong_monotonic[i][y1];
            Monotonic_element& prev = cong_monotonic[i - 1][y1];
            ele.max_cost = std::max(cost, prev.max_cost);
            ele.total_cost = prev.total_cost + std::max(static_cast<double>(0), cost);
            ele.distance = prev.distance + distance;
            ele.via_num = prev.via_num;

            if (!bound.flag || (bound.flag && smaller_than_lower_bound(ele, bound))) {
                parent_monotonic[i][y1] = LEFT;
            } else
                parent_monotonic[i][y1] = -2;
        } else
            parent_monotonic[i][y1] = -2;
    }

//If source is in the left-top corner
    if (dir == BACK) {
        for (j = y1 + 1; j <= y2; ++j) {
            if (parent_monotonic[x1][j - 1] != -2) {
                cost = congestion.get_cost_2d(x1, j, dir, net_id, &distance);
                cong_monotonic[x1][j].max_cost = std::max(cost, cong_monotonic[x1][j - 1].max_cost);
                cong_monotonic[x1][j].total_cost = cong_monotonic[x1][j - 1].total_cost + std::max(static_cast<double>(0), cost);
                cong_monotonic[x1][j].distance = cong_monotonic[x1][j - 1].distance + distance;
                cong_monotonic[x1][j].via_num = cong_monotonic[x1][j - 1].via_num;
                if (!bound.flag || (bound.flag && smaller_than_lower_bound(cong_monotonic[x1][j], bound))) {
                    parent_monotonic[x1][j] = dir;
                } else
                    parent_monotonic[x1][j] = dir;
            } else
                parent_monotonic[x1][j] = -2;
        }

//If source is in the left-bottom corner
    } else if (dir == FRONT) {
        for (j = y1 - 1; j >= y2; --j) {
            if (parent_monotonic[x1][j + 1] != -2) {
                cost = congestion.get_cost_2d(x1, j, dir, net_id, &distance);
                cong_monotonic[x1][j].max_cost = std::max(cost, cong_monotonic[x1][j + 1].max_cost);
                cong_monotonic[x1][j].total_cost = cong_monotonic[x1][j + 1].total_cost + std::max(static_cast<double>(0), cost);
                cong_monotonic[x1][j].distance = cong_monotonic[x1][j + 1].distance + distance;
                cong_monotonic[x1][j].via_num = cong_monotonic[x1][j + 1].via_num;
                if (!bound.flag || (bound.flag && smaller_than_lower_bound(cong_monotonic[x1][j], bound))) {
                    parent_monotonic[x1][j] = dir;
                } else
                    parent_monotonic[x1][j] = -2;
            } else
                parent_monotonic[x1][j] = -2;
        }
    }

    for (i = x1 + 1; i <= x2; ++i) {
//If source is in the left-bottom corner
        if (dir == BACK) {
            for (j = y1 + 1; j <= y2; ++j)
                compare_two_direction_congestion(i, j, LEFT, i - 1, dir, j - 1, net_id, bound);

            //If source is in the left-top corner
        } else {
            for (j = y1 - 1; j >= y2; --j)
                compare_two_direction_congestion(i, j, LEFT, i - 1, dir, j + 1, net_id, bound);
        }
    }
}

void MonotonicRouting::traverse_parent_monotonic(int x1, int y1, int x2, int y2, Two_pin_element_2d& two_pin_monotonic_path) {
    int i = x2;
    int j = y2;
//Sink != Source
    while ((i != x1) || (j != y1)) {
//Push the path in to a list
        two_pin_monotonic_path.path.push_back(Coordinate_2d(i, j));

//Update the coordinate of tracing cell
        if (parent_monotonic[i][j] == LEFT)
            --i;
        else if (parent_monotonic[i][j] == FRONT)
            ++j;
        else
            --j;
    }

//push the source to list
    two_pin_monotonic_path.path.push_back(Coordinate_2d(i, j));
}

//Try to obtain a monotonic routing path without cost over bounding cost
//Return true if there exist one such path
bool MonotonicRouting::monotonic_pattern_route(int x1, int y1, int x2, int y2, Two_pin_element_2d& two_pin_monotonic_path, int net_id, Bound& bound) {
    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    if (y1 <= y2) //s->t RIGHT and FRONT (source is in the left-bottom corner)
        monotonic_routing_algorithm(x1, y1, x2, y2, BACK, net_id, bound);
// use x1,y+1.edge_list[back]
    else if (y1 > y2) //s->t RIGHT and BACK (source is in the left-top corner)
        monotonic_routing_algorithm(x1, y1, x2, y2, FRONT, net_id, bound);

//If there is no solution for this 2-pin net, return false
    if (parent_monotonic[x2][y2] == -2)
        return false;

//travese parent_monotonic to find path in two_pin_monotonic_path
    traverse_parent_monotonic(x1, y1, x2, y2, two_pin_monotonic_path);

    two_pin_monotonic_path.pin1 = two_pin_monotonic_path.path[0];
    two_pin_monotonic_path.pin2 = two_pin_monotonic_path.path.back();
    two_pin_monotonic_path.net_id = net_id;
    return true;
}

/*
 Compare two cost and return a pointer to the Monotonici_element which has smaller cost
 */
const bool Monotonic_element::operator <(Monotonic_element& m2) const {
    if ((total_cost - m2.total_cost) < (neg_error_bound))
        return true;
    else if ((total_cost - m2.total_cost) > (neg_error_bound))
        return false;
    else {
        if ((max_cost - m2.max_cost) < (neg_error_bound))
            return true;
        else if ((max_cost - m2.max_cost) > (neg_error_bound))
            return false;
        else {
            if (distance < m2.distance)
                return true;
            else if (distance > m2.distance)
                return false;
            else {
                if (via_num <= m2.via_num)
                    return true;
                else
                    return false;
            }
        }
    }
}

void MonotonicRouting::compare_two_direction_congestion(int i, int j, OrientationType dir1, int pre_i, OrientationType dir2, int pre_j, int net_id, Bound& bound) {
    Monotonic_element left_element;
    Monotonic_element vertical_element;
    Monotonic_element* choose_element;
    double cost;
    int distance = 1;

    int pre_dir;

    bool left_flag = true;
    bool right_flag = true;
    if (parent_monotonic[pre_i][j] != -2) {
        cost = congestion.get_cost_2d(i, j, dir1, net_id, &distance);
        left_element.max_cost = std::max(cost, cong_monotonic[pre_i][j].max_cost);
        left_element.total_cost = cong_monotonic[pre_i][j].total_cost + std::max(static_cast<double>(0), cost);
        left_element.distance = cong_monotonic[pre_i][j].distance + distance;
        pre_dir = parent_monotonic[pre_i][j];
        if (pre_dir < 2) {
            left_element.via_num = cong_monotonic[pre_i][j].via_num + congestion.via_cost;
            if (distance != 0) {
                left_element.distance += congestion.via_cost;
                if (congestion.used_cost_flag == HISTORY_COST)
                    left_element.total_cost += congestion.via_cost;
            }
        } else
            left_element.via_num = cong_monotonic[pre_i][j].via_num;
        if (!bound.flag || (bound.flag && smaller_than_lower_bound(left_element, bound))) {
            left_flag = true;
        } else
            left_flag = false;
    } else
        left_flag = false;
    if (parent_monotonic[i][pre_j] != -2) {
        cost = congestion.get_cost_2d(i, j, dir2, net_id, &distance);
        vertical_element.max_cost = std::max(cost, cong_monotonic[i][pre_j].max_cost);
        vertical_element.total_cost = cong_monotonic[i][pre_j].total_cost + std::max(static_cast<double>(0), cost);
        vertical_element.distance = cong_monotonic[i][pre_j].distance + distance;
        pre_dir = parent_monotonic[i][pre_j];
        if (pre_dir >= 2) {
            vertical_element.via_num = cong_monotonic[i][pre_j].via_num + congestion.via_cost;
            if (distance != 0) {
                vertical_element.distance += congestion.via_cost;
                if (congestion.used_cost_flag == HISTORY_COST)
                    vertical_element.total_cost += congestion.via_cost;
            }
        } else
            vertical_element.via_num = cong_monotonic[i][pre_j].via_num;
        if (!bound.flag || (bound.flag && smaller_than_lower_bound(vertical_element, bound))) {
            right_flag = true;
        } else
            right_flag = false;
    } else
        right_flag = false;

    if ((!left_flag) && (!right_flag)) {
        parent_monotonic[i][j] = -2;
        return;
    } else if (left_flag && right_flag)
        choose_element = compare_cost(&left_element, &vertical_element);
    else if (left_flag)
        choose_element = &left_element;
    else
        choose_element = &vertical_element;

    cong_monotonic[i][j] = *choose_element;

    if (choose_element == (&left_element))
        parent_monotonic[i][j] = dir1;
    else if (choose_element == (&vertical_element))
        parent_monotonic[i][j] = dir2;
    else {
        puts("compare has problem!!!\n");
        exit(0);
    }
}
bool MonotonicRouting::smaller_than_lower_bound(const Monotonic_element& m, Bound& bound) {
    if ((m.total_cost - bound.cost) < neg_error_bound)
        return true;
    else if ((m.total_cost - bound.cost) > error_bound)
        return false;
    else {
        if (m.distance < bound.distance)
            return true;
        else if (m.distance > bound.distance)
            return false;
        else {
            return (m.via_num < bound.via_num);
        }
    }
}

//Obtain a cost of a path, including via cost.
void MonotonicRouting::compute_path_total_cost_and_distance(Two_pin_element_2d& element, Monotonic_element& mn) {
    int distance;
    int pre_dir = -1;

    mn.total_cost = 0;
    mn.distance = 0;
    mn.via_num = 0;
    for (int i = element.path.size() - 2; i >= 0; --i) {
        Coordinate_2d& c = element.path[i];

        OrientationType dir = Coordinate_2d::get_direction(c, element.path[i + 1]);
        mn.total_cost += congestion.get_cost_2d(c.x, c.y, dir, element.net_id, &distance);
        mn.distance += distance;
        if (pre_dir != -1) {
            //if the wire need to bend, then we need to add via cost to it
            if ((pre_dir < 2 && dir >= 2) || (pre_dir >= 2 && dir < 2)) {
                mn.via_num += congestion.via_cost;
                if (congestion.used_cost_flag == HISTORY_COST) {
                    mn.total_cost += congestion.via_cost;
                }
            }
        }
        pre_dir = dir;
    }
}

bool MonotonicRouting::monotonicRoute(Two_pin_element_2d& two_pin, Bound& bound, std::vector<Coordinate_2d>& bound_path) {
    Monotonic_element mn;
    compute_path_total_cost_and_distance(two_pin, mn);

    bound.cost = mn.total_cost;
    bound.distance = mn.distance;
    bound.via_num = mn.via_num;
    bound.flag = true;
    two_pin.path.clear();

    bool find_path_flag = false;

    if (monotonic_enable) {
        bool find_path_flag = monotonic_pattern_route(two_pin.pin1.x, two_pin.pin1.y, two_pin.pin2.x, two_pin.pin2.y, two_pin, two_pin.net_id, bound);

        if (find_path_flag) {
            bound_path.clear();
            bound.cost = cong_monotonic[two_pin.path[0].x][two_pin.path[0].y].total_cost;
            bound.distance = cong_monotonic[two_pin.path[0].x][two_pin.path[0].y].distance;
            bound.via_num = cong_monotonic[two_pin.path[0].x][two_pin.path[0].y].via_num;
        }
    }
    return find_path_flag;
}