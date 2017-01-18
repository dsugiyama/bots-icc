#!/usr/bin/env ruby

require 'optparse'

workload = 'small'
task_type = 'both'
num_threads = []
skip_serial = false
niter = 1
membind = "";
label = "icc"

parser = OptionParser.new
parser.on('-w', '--workload=VAL', 'Name of the workload') { |v| workload = v }
parser.on('-t', '--task-type=VAL', 'Type of OpenMP tasks ("tied", "untied", or "both")') { |v| task_type = v }
parser.on('-n', '--num-threads=VAL', 'Number of threads') { |v| num_threads = v.split(',').map(&:to_i) }
parser.on('--skip-serial', 'Skip the execution of serial version') { skip_serial = true }
parser.on('-i', '--num-iterations=VAL', 'Comma-separated list of number of iterations') { |v| niter = v }
parser.on('--knl', 'Whether executed on KNL') { membind = "numactl --membind 1" }
parser.on('-l', '--label=VAL', 'Label of executables') { |v| label = v }
parser.on_tail('-h', '--help', 'Show this message') { puts parser; exit }
parser.parse!(ARGV)

puts 'unit: nodes/sec'
puts "# of threads: #{num_threads.join(' ')}"
puts

def run(command, num_iterations)
  iter_cmd = "for i in $(seq #{num_iterations}); do #{command}; done"
  stack_size_kb = 32 * 1024
  settings  = "ulimit -s #{stack_size_kb}; export OMP_STACKSIZE=#{stack_size_kb}; "
  settings += "export OMP_WAIT_POLICY=ACTIVE; "
  settings += "export KMP_TASK_STEALING_CONSTRAINT=0;"
  result = `#{settings} #{iter_cmd}`
    .scan(/Nodes\/Sec\s*= ([0-9\.]+)/)
    .flatten
    .map(&:to_f)
    .max
  puts result
end

unless skip_serial
  puts 'serial'
  run "#{membind} bin/uts.#{label}.serial -f inputs/uts/#{workload}.input", niter
  puts
end

if task_type == 'untied' || task_type == 'both'
  puts 'untied'
  num_threads.each do |n|
    run "OMP_NUM_THREADS=#{n} #{membind} bin/uts.#{label}.omp-tasks -f inputs/uts/#{workload}.input", niter
  end
  puts
end

if task_type == 'tied' || task_type == 'both'
  puts 'tied'
  num_threads.each do |n|
    run "OMP_NUM_THREADS=#{n} #{membind} bin/uts.#{label}.omp-tasks-tied -f inputs/uts/#{workload}.input", niter
  end
  puts
end
