//
// Created by kier on 2018/7/18.
//

#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <utility>
#include <climits>
#include <algorithm>

#include <module/module.h>


#include "module/module.h"
#include "utils/box.h"
#include "core/tensor_builder.h"
#include "module/io/fstream.h"
#include "module/menu.h"
#include "module/header.h"

namespace ts {

    void Module::load(Graph g) {
        auto nodes = g.nodes();
        std::vector<Node> outputs;
        for (auto &node : nodes) {
            if (node.outputs().empty()) {
                outputs.push_back(node);
            }
        }
        load(g, outputs);
    }

    void Module::load(Graph g, const std::vector<Node> &outputs) {
        auto inputs = graph_walker(g, outputs);

        m_inputs.insert(m_inputs.end(), inputs.begin(), inputs.end());
        m_outputs.insert(m_outputs.end(), outputs.begin(), outputs.end());
        m_graphs.push_back(g);
    }

    void Module::load(Graph g, const std::vector<std::string> &outputs) {
        std::unordered_map<std::string, Node> map_name_found_node;
        size_t found_count = 0;
        Graph local_graph;
        Node empty_node = local_graph.make("_empty");
        for (auto &output_name : outputs) {
            map_name_found_node.insert(std::make_pair(output_name, empty_node));
        }
        auto nodes = g.nodes();
        for (auto &node : nodes) {
            auto &bubble = node.bubble();
            auto name_it = map_name_found_node.find(bubble.name());
            if (name_it == map_name_found_node.end()) {
                continue;
            }
            if (name_it->second != empty_node) {
                TS_LOG_ERROR << "Found duplicate Node " << node.str() << ", with Node " << name_it->second.str()
                             << eject;
            }
            ++found_count;
            name_it->second = node;
        }
        if (found_count != map_name_found_node.size()) {
            std::ostringstream oss;
            oss << "Can not found those names in graph: ";
            size_t not_found_name_count = 0;
            for (auto &name_found_node_pair : map_name_found_node) {
                if (name_found_node_pair.second != empty_node) continue;
                if (not_found_name_count) oss << ", ";
                oss << name_found_node_pair.first;
                ++not_found_name_count;
            }
            TS_LOG_ERROR(oss.str()) << eject;
        }
        std::vector<Node> found_nodes;
        found_nodes.reserve(outputs.size());
        for (auto &output_name : outputs) {
            Node &found_node = map_name_found_node.at(output_name);
            found_nodes.emplace_back(found_node);
        }
        this->load(g, found_nodes);
    }

    std::vector<Node> Module::graph_walker(Graph g, const std::vector<Node> &outputs) {
        // calculate inputs and outputs
        auto nodes = g.nodes();
        std::unordered_set<Node> nodes_set;
        for (auto &node : nodes) {
            nodes_set.insert(node);
        }
        // valid graph, get inputs
        std::vector<Node> inputs;
        std::queue<Node> walker;
        std::unordered_set<Node> walked;
        for (auto &node : outputs) walker.push(node);
        while (!walker.empty()) {
            auto node = walker.front();
            walker.pop();
            if (walked.find(node) != walked.end()) continue;
            walked.insert(node);
            if (nodes_set.find(node) == nodes_set.end()) {
                TS_LOG_ERROR << "Found unlinked node in graph: " << node.str() << eject;
            }
            auto &op = node.bubble();
            if (op.op() == Bubble::Parameter) {
                inputs.push_back(node);
                continue;
            }
            if (op.op() == Bubble::Const) {
                continue;
            }
            if (node.inputs().empty()) {
                TS_LOG_ERROR << "Found not computable node: " << node.str() << eject;
            }
            for (auto &input : node.inputs()) {
                walker.push(input);
            }
        }
        // remove duplicate inputs
        std::unordered_set<Node> input_nodes_set;
        for (auto &input : inputs) input_nodes_set.insert(input);

        // return non-duplicate nodes
        return std::vector<Node>(input_nodes_set.begin(), input_nodes_set.end());
    }

    void Module::clear() {
        m_inputs.clear();
        m_outputs.clear();
        m_graphs.clear();
    }

    void Module::sort_inputs(const std::vector<Node> &inputs) {
        std::unordered_set<Node> sorted_input_set(inputs.begin(), inputs.end());
        for (auto &had_input : m_inputs) {
            if (sorted_input_set.find(had_input) == sorted_input_set.end()) {
                TS_LOG_ERROR << "The sorted inputs must content " << had_input.str() << eject;
            }
        }
        m_inputs = inputs;
    }

    void Module::sort_inputs(const std::vector<std::string> &input_names) {
        std::unordered_map<std::string, Node> map_name_input_node;
        // map nodes
        for (auto &input : m_inputs) {
            auto &bubble = input.ref<Bubble>();
            if (map_name_input_node.find(bubble.name()) != map_name_input_node.end()) {
                auto it = map_name_input_node.find(bubble.name());
                TS_LOG_ERROR << "Can not sort inputs with duplicate names: " << input.str() << " and "
                             << it->second.str() << eject;
            }
            map_name_input_node.insert(std::make_pair(bubble.name(), input));
        }
        // check inputs node
        std::unordered_set<Node> used_inputs;   // make sure all inputs must be used after used
        std::vector<Node> sorted_inputs;
        for (auto &input_name : input_names) {
            auto name_node_it = map_name_input_node.find(input_name);
            if (name_node_it == map_name_input_node.end()) {
                TS_LOG_ERROR << "Can not recognize name " << input_name << eject;
            }
            auto &node = name_node_it->second;
            sorted_inputs.emplace_back(node);
            used_inputs.insert(node);
        }
        if (used_inputs.size() < map_name_input_node.size()) {
            std::ostringstream oss;
            oss << "All inputs must be used after sorted, missing: ";
            size_t missing_count = 0;
            for (auto &name_node_pair : map_name_input_node) {
                auto &node = name_node_pair.second;
                if (used_inputs.find(node) != used_inputs.end()) continue;
                if (missing_count) oss << ", ";
                oss << name_node_pair.first;
                ++missing_count;
            }
            TS_LOG_ERROR(oss.str()) << eject;
        }
        m_inputs = sorted_inputs;
    }

    void Module::sort_inputs(const std::initializer_list<std::string> &input_names) {
        this->sort_inputs(std::vector<std::string>(input_names.begin(), input_names.end()));
    }

    template <typename K, typename V>
    using map = std::unordered_map<K, V>;
    template <typename K>
    using set = std::unordered_set<K>;

    std::vector<std::pair<Node, int>> Module::list_reference_nodes(const std::vector<Node> &nodes) {
        map<Node, int> map_node_depth;
        std::deque<Node> node_walker; // top_down

        for (auto &node : nodes) {
            auto it = map_node_depth.find(node);
            if (it != map_node_depth.end()) continue;
            node_walker.push_back(node);
            map_node_depth.insert(std::make_pair(node, 1));
        }

        while (!node_walker.empty()) {
            auto node = node_walker.front();
            node_walker.pop_front();
            auto depth = map_node_depth[node];
            for (auto &input : node.inputs()) {
                auto input_depth_pair = map_node_depth.find(input);
                if (input_depth_pair == map_node_depth.end()) {
                    map_node_depth.insert(std::make_pair(input, depth + 1));
                    node_walker.push_back(input);
                } else {
                    auto this_input_depth = depth + 1;
                    if (input_depth_pair->second < this_input_depth) {
                        input_depth_pair->second = this_input_depth;
                        node_walker.push_back(input);   // re-walk nodes
                    }
                }
            }
        }

        std::vector<std::pair<Node, int>> computation_schedule(map_node_depth.begin(), map_node_depth.end());
        std::sort(computation_schedule.begin(), computation_schedule.end(),
                  [](const std::pair<Node, int> &lhs, const std::pair<Node, int> &rhs){
                      return lhs.second > rhs.second;
                  });

        return std::move(computation_schedule);
    }

    void Module::Save(const std::string &filename, Module::shared module, Module::SerializationFormat format) {
        TS_AUTO_CHECK(format == BINARY);
        // get nodes ready to read
        FileStreamWriter stream(filename);
        auto valued_nodes = list_reference_nodes(module->outputs());
        std::vector<Node> nodes;
        std::unordered_map<Node, size_t> map_node_index;
        size_t index = 0;
        for (auto &valued_node : valued_nodes) {
            auto &node = valued_node.first;
            map_node_index.insert(std::make_pair(node, index++));
            nodes.emplace_back(node);
        }
        for (auto &input : module->inputs()) {
            if (map_node_index.find(input) != map_node_index.end()) continue;
            auto &node = input;
            map_node_index.insert(std::make_pair(node, index++));
            nodes.emplace_back(node);
        }

        // 0. save header
        Header header;
        header.code = TS_MODULE_CODE_V1;
        header.serialize(stream);

        // 1. save inputs
        binio::write<uint32_t>(stream, uint32_t(module->inputs().size()));
        for (auto &node : module->inputs()) {
            binio::write<uint32_t>(stream, uint32_t(map_node_index[node]));
        }
        // 2. save outputs
        binio::write<uint32_t>(stream, uint32_t(module->outputs().size()));
        for (auto &node : module->outputs()) {
            binio::write<uint32_t>(stream, uint32_t(map_node_index[node]));
        }
        // 3. save graphs
        serialize_nodes(stream, nodes);
    }

    static size_t read_uint32_list(StreamReader &stream, std::vector<uint32_t> &list) {
        uint32_t size_buffer = 0;
        size_t read_size = 0;
        read_size += binio::read<uint32_t>(stream, size_buffer);
        list.resize(size_buffer);
        for (auto &elem : list) {
            read_size += binio::read<uint32_t>(stream, elem);
        }
        return read_size;
    }

    Module::shared Module::Load(const std::string &filename, Module::SerializationFormat format) {
        TS_AUTO_CHECK(format == BINARY);
        FileStreamReader stream(filename);
        size_t read_size = 0;

        // 0. read header
        Header header;
        read_size += header.externalize(stream);
        TS_AUTO_CHECK(header.code == TS_MODULE_CODE_V1);

        // 1. read inputs
        // read node index
        std::vector<uint32_t> input_index;
        read_size += read_uint32_list(stream, input_index);
        // 2. read outputs
        std::vector<uint32_t> output_index;
        read_size += read_uint32_list(stream, output_index);
        // 3. read graph
        Graph g;
        read_size += externalize_graph(stream, g);
        const auto &nodes = g.nodes();  // TODO: Check if the read nodes is the given nodes
        // x.1 convert inputs and outputs
        std::vector<Node> inputs;
        for (auto index : input_index) inputs.emplace_back(nodes[index]);
        std::vector<Node> outputs;
        for (auto index : output_index) outputs.emplace_back(nodes[index]);
        Module::shared module = std::make_shared<Module>();
        module->load(g, outputs);
        module->sort_inputs(inputs);
        return module;
    }
}
