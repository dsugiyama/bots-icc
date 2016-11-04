#!/usr/bin/env ruby

require 'optparse'

workload = 'small'
task_type = 'both'
num_threads = []
skip_serial = false
niter = 1

parser = OptionParser.new
parser.on('-w', '--workload=VAL') { |v| workload = v }
parser.on('-t', '--task-type=VAL') { |v| task_type = v }
parser.on('-n', '--num-threads=VAL') { |v| num_threads = v.split(',').map(&:to_i) }
parser.on('--skip-serial') { skip_serial = true }
parser.on('-i', '--num-iterations=VAL') { |v| niter = v }
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
  run "bin/uts.icc.serial -f inputs/uts/#{workload}.input", niter
  puts
end

if task_type == 'untied' || task_type == 'both'
  puts 'untied'
  num_threads.each do |n|
    run "OMP_NUM_THREADS=#{n} bin/uts.icc.omp-tasks -f inputs/uts/#{workload}.input", niter
  end
  puts
end

if task_type == 'tied' || task_type == 'both'
  puts 'tied'
  num_threads.each do |n|
    run "OMP_NUM_THREADS=#{n} bin/uts.icc.omp-tasks-tied -f inputs/uts/#{workload}.input", niter
  end
  puts
end
