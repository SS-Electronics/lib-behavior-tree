/**
 * @file   conf_behaviour_tree.h
 * @brief  lib-behavior-tree configuration for CI POSIX builds.
 * @author Subhajit Roy <subhajitroy005@gmail.com>
 *
 * This file is used only by the CI pipeline (build-posix, cppcheck).
 * Do not copy this into application projects — start from inc/conf_behaviour_tree.h.
 */
#ifndef CONF_BEHAVIOUR_TREE_H
#define CONF_BEHAVIOUR_TREE_H

#define BT_PLATFORM                  BT_PLATFORM_POSIX
#define BT_CHILDREN_INITIAL_CAPACITY 4
#define BT_ENABLE_NODE_NAMES         1
#define BT_ENABLE_STATS              1

#endif /* CONF_BEHAVIOUR_TREE_H */
